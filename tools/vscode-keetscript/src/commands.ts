import * as vscode from 'vscode';
import { DiagnosticManager } from './diagnostics';

export class CommandManager {
  private terminal: vscode.Terminal | null = null;

  constructor(private diagnosticManager: DiagnosticManager | null) {}

  public registerCommands(context: vscode.ExtensionContext): void {
    context.subscriptions.push(
      vscode.commands.registerCommand('keetscript.run', () => this.runCurrentFile()),
      vscode.commands.registerCommand('keetscript.build', () => this.buildCurrentFile()),
      vscode.commands.registerCommand('keetscript.check', () => this.checkCurrentFile())
    );
  }

  private getTerminal(): vscode.Terminal {
    if (!this.terminal || this.terminal.exitStatus !== undefined) {
      this.terminal = vscode.window.createTerminal('KeetScript');
    }
    return this.terminal;
  }

  private async ensureSaved(document: vscode.TextDocument): Promise<void> {
    if (document.isDirty) {
      await document.save();
    }
  }

  private async runCurrentFile(): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showWarningMessage('No active KeetScript file to run.');
      return;
    }

    if (editor.document.languageId !== 'keetscript') {
      vscode.window.showWarningMessage('Active file is not a KeetScript (.keet) file.');
      return;
    }

    await this.ensureSaved(editor.document);

    const compiler = this.resolveCompilerPath();
    const filePath = editor.document.uri.fsPath;
    const terminal = this.getTerminal();

    terminal.show();
    terminal.sendText(`${this.escapeShellArg(compiler)} run ${this.escapeShellArg(filePath)}`);
  }

  private async buildCurrentFile(): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showWarningMessage('No active KeetScript file to build.');
      return;
    }

    if (editor.document.languageId !== 'keetscript') {
      vscode.window.showWarningMessage('Active file is not a KeetScript (.keet) file.');
      return;
    }

    await this.ensureSaved(editor.document);

    const compiler = this.resolveCompilerPath();
    const filePath = editor.document.uri.fsPath;
    const terminal = this.getTerminal();

    terminal.show();
    terminal.sendText(`${this.escapeShellArg(compiler)} build ${this.escapeShellArg(filePath)}`);
  }

  private async checkCurrentFile(): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      return;
    }

    if (editor.document.languageId !== 'keetscript') {
      return;
    }

    if (this.diagnosticManager) {
      await this.diagnosticManager.checkDocument(editor.document);
      vscode.window.setStatusBarMessage('KeetScript check complete', 2000);
    } else {
      vscode.window.setStatusBarMessage('KeetScript: Live LSP check active', 2000);
    }
  }

  private resolveCompilerPath(): string {
    return this.diagnosticManager ? this.diagnosticManager.resolveCompilerPath() : 'keety';
  }

  private escapeShellArg(arg: string): string {
    if (arg.includes(' ') || arg.includes('\t')) {
      return `"${arg.replace(/"/g, '\\"')}"`;
    }
    return arg;
  }
}
