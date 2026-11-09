import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import * as cp from 'child_process';
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind
} from 'vscode-languageclient/node';

export class LspClientManager implements vscode.Disposable {
  private client: LanguageClient | null = null;
  private outputChannel: vscode.OutputChannel;

  constructor() {
    this.outputChannel = vscode.window.createOutputChannel('KeetScript Language Server');
  }

  public resolveLspServerPath(): string | null {
    const config = vscode.workspace.getConfiguration('keetscript');
    const configuredPath = config.get<string>('lspServerPath', 'keet-lsp');

    // 1. Explicit configured path
    if (configuredPath && configuredPath !== 'keet-lsp') {
      if (path.isAbsolute(configuredPath) && fs.existsSync(configuredPath)) {
        return configuredPath;
      }
      if (vscode.workspace.workspaceFolders) {
        for (const folder of vscode.workspace.workspaceFolders) {
          const resolved = path.join(folder.uri.fsPath, configuredPath);
          if (fs.existsSync(resolved)) {
            return resolved;
          }
        }
      }
    }

    // 2. Auto-detect in workspace folders
    if (vscode.workspace.workspaceFolders) {
      for (const folder of vscode.workspace.workspaceFolders) {
        const candidates = [
          path.join(folder.uri.fsPath, 'build', 'src', 'keet-lsp'),
          path.join(folder.uri.fsPath, 'build', 'keet-lsp'),
          path.join(folder.uri.fsPath, 'bin', 'keet-lsp')
        ];
        for (const cand of candidates) {
          if (fs.existsSync(cand)) {
            return cand;
          }
        }
      }
    }

    // 3. Extension-relative build directories
    const extCandidates = [
      path.resolve(__dirname, '..', '..', '..', 'build', 'src', 'keet-lsp'),
      path.resolve(__dirname, '..', '..', '..', 'build', 'keet-lsp'),
      path.resolve(__dirname, '..', '..', '..', 'bin', 'keet-lsp')
    ];
    for (const cand of extCandidates) {
      if (fs.existsSync(cand)) {
        return cand;
      }
    }

    // 4. System PATH search
    try {
      const checkCmd = process.platform === 'win32' ? 'where keet-lsp' : 'which keet-lsp';
      const out = cp.execSync(checkCmd, { encoding: 'utf8', stdio: ['pipe', 'pipe', 'ignore'] }).trim();
      if (out && fs.existsSync(out)) {
        return out;
      }
    } catch {
      // not in PATH
    }

    return null;
  }

  public async start(): Promise<boolean> {
    const config = vscode.workspace.getConfiguration('keetscript');
    const enabled = config.get<boolean>('enableLsp', true);
    if (!enabled) {
      this.outputChannel.appendLine('KeetScript LSP is disabled in settings (keetscript.enableLsp = false).');
      return false;
    }

    const serverPath = this.resolveLspServerPath();
    if (!serverPath) {
      this.outputChannel.appendLine('keet-lsp binary not found. Falling back to compiler check-on-save mode.');
      return false;
    }

    this.outputChannel.appendLine(`Found keet-lsp binary at: ${serverPath}`);

    const serverOptions: ServerOptions = {
      run: { command: serverPath, transport: TransportKind.stdio },
      debug: { command: serverPath, transport: TransportKind.stdio }
    };

    const clientOptions: LanguageClientOptions = {
      documentSelector: [
        { scheme: 'file', language: 'keetscript' },
        { scheme: 'file', language: 'kml' }
      ],
      synchronize: {
        fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{keet,kml}')
      }
    };

    try {
      this.client = new LanguageClient(
        'keetscript-lsp',
        'KeetScript Language Server',
        serverOptions,
        clientOptions
      );

      await this.client.start();
      this.outputChannel.appendLine('KeetScript Language Server started successfully.');
      return true;
    } catch (err) {
      this.outputChannel.appendLine(`Failed to start Language Server: ${err}. Falling back to check mode.`);
      if (this.client) {
        try {
          await this.client.stop();
        } catch {
          // ignore
        }
        this.client = null;
      }
      return false;
    }
  }

  public async stop(): Promise<void> {
    if (this.client) {
      await this.client.stop();
      this.client = null;
    }
  }

  public isRunning(): boolean {
    return this.client !== null;
  }

  public dispose(): void {
    if (this.client) {
      this.client.stop();
      this.client = null;
    }
    this.outputChannel.dispose();
  }
}
