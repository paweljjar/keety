import * as vscode from 'vscode';
import * as cp from 'child_process';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';

export interface JsonDiagnosticLocation {
  file: string;
  line: number;
  column: number;
  endLine: number;
  endColumn: number;
  offset: number;
  length: number;
}

export interface JsonDiagnosticLabel {
  file: string;
  line: number;
  column: number;
  endLine: number;
  endColumn: number;
  message: string;
  isPrimary: boolean;
}

export interface JsonDiagnosticSuggestion {
  type: string;
  file: string;
  startLine: number;
  startColumn: number;
  endLine: number;
  endColumn: number;
  replacement: string;
  title: string;
}

export interface JsonDiagnostic {
  code: string;
  severity: string;
  message: string;
  location: JsonDiagnosticLocation;
  labels: JsonDiagnosticLabel[];
  notes: string[];
  helps: string[];
  suggestions: JsonDiagnosticSuggestion[];
}

export interface KeetDiagnostic extends vscode.Diagnostic {
  rawJson?: JsonDiagnostic;
}

export class DiagnosticManager implements vscode.Disposable {
  private diagnosticCollection: vscode.DiagnosticCollection;
  private debounceTimers: Map<string, NodeJS.Timeout> = new Map();
  private disposables: vscode.Disposable[] = [];
  private statusBarItem: vscode.StatusBarItem;

  constructor() {
    this.diagnosticCollection = vscode.languages.createDiagnosticCollection('keetscript');
    this.statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
    this.statusBarItem.command = 'workbench.actions.view.problems';
    this.statusBarItem.text = '$(check) Keet: OK';
    this.statusBarItem.tooltip = 'KeetScript Diagnostics Status';
    this.statusBarItem.show();

    this.registerListeners();
  }

  private registerListeners(): void {
    this.disposables.push(
      vscode.workspace.onDidOpenTextDocument(doc => {
        if (doc.languageId === 'keetscript') {
          this.checkDocument(doc);
        }
      }),
      vscode.workspace.onDidSaveTextDocument(doc => {
        if (doc.languageId === 'keetscript') {
          const config = vscode.workspace.getConfiguration('keetscript');
          if (config.get<boolean>('checkOnSave', true)) {
            this.clearDebounce(doc.uri.toString());
            this.checkDocument(doc);
          }
        }
      }),
      vscode.workspace.onDidChangeTextDocument(event => {
        if (event.document.languageId === 'keetscript') {
          const config = vscode.workspace.getConfiguration('keetscript');
          if (config.get<boolean>('checkOnType', true)) {
            const delay = config.get<number>('checkDelayMs', 300);
            this.scheduleCheck(event.document, delay);
          }
        }
      }),
      vscode.workspace.onDidCloseTextDocument(doc => {
        this.clearDebounce(doc.uri.toString());
        this.diagnosticCollection.delete(doc.uri);
        this.updateStatusBar();
      }),
      vscode.window.onDidChangeActiveTextEditor(editor => {
        if (editor && editor.document.languageId === 'keetscript') {
          this.updateStatusBar();
        }
      })
    );

    // Initial check for open documents
    for (const doc of vscode.workspace.textDocuments) {
      if (doc.languageId === 'keetscript') {
        this.checkDocument(doc);
      }
    }
  }

  private scheduleCheck(document: vscode.TextDocument, delay: number): void {
    const key = document.uri.toString();
    this.clearDebounce(key);
    const timer = setTimeout(() => {
      this.checkDocument(document);
    }, delay);
    this.debounceTimers.set(key, timer);
  }

  private clearDebounce(key: string): void {
    const existing = this.debounceTimers.get(key);
    if (existing) {
      clearTimeout(existing);
      this.debounceTimers.delete(key);
    }
  }

  public resolveCompilerPath(): string {
    const config = vscode.workspace.getConfiguration('keetscript');
    const configuredPath = config.get<string>('compilerPath', 'keety');

    if (configuredPath && configuredPath !== 'keety') {
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

    // Auto-detect in workspace build/src/keety or build/keety
    if (vscode.workspace.workspaceFolders) {
      for (const folder of vscode.workspace.workspaceFolders) {
        const candidates = [
          path.join(folder.uri.fsPath, 'build', 'src', 'keety'),
          path.join(folder.uri.fsPath, 'build', 'keety'),
          path.join(folder.uri.fsPath, 'bin', 'keety')
        ];
        for (const cand of candidates) {
          if (fs.existsSync(cand)) {
            return cand;
          }
        }
      }
    }

    // Check extension relative builds (e.g. tools/vscode-keetscript/../../build/src/keety)
    const extCandidates = [
      path.resolve(__dirname, '..', '..', '..', 'build', 'src', 'keety'),
      path.resolve(__dirname, '..', '..', '..', 'build', 'keety')
    ];
    for (const cand of extCandidates) {
      if (fs.existsSync(cand)) {
        return cand;
      }
    }

    return 'keety';
  }

  public async checkDocument(document: vscode.TextDocument): Promise<void> {
    if (document.languageId !== 'keetscript') {
      return;
    }

    const compiler = this.resolveCompilerPath();
    const config = vscode.workspace.getConfiguration('keetscript');
    const extraArgs = config.get<string[]>('extraCheckArgs', []);

    let targetFile = document.uri.fsPath;
    let isTemp = false;

    // If document is dirty or untitled, write to a temp file
    if (document.isDirty || document.isUntitled) {
      isTemp = true;
      const tmpDir = os.tmpdir();
      const safeName = `keet_${Date.now()}_${path.basename(document.fileName || 'untitled.keet')}`;
      targetFile = path.join(tmpDir, safeName);
      try {
        fs.writeFileSync(targetFile, document.getText(), 'utf8');
      } catch {
        targetFile = document.uri.fsPath;
        isTemp = false;
      }
    }

    const args = ['check', '--diagnostic-format=json', ...extraArgs, targetFile];
    const cwd = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || path.dirname(document.uri.fsPath);

    cp.execFile(
      compiler,
      args,
      { cwd, maxBuffer: 10 * 1024 * 1024 },
      (_error, stdout, _stderr) => {
        if (isTemp) {
          try {
            fs.unlinkSync(targetFile);
          } catch {
            // ignore temp cleanup error
          }
        }

        const rawOutput = (stdout || '').trim();
        let jsonItems: JsonDiagnostic[] = [];

        try {
          if (rawOutput.startsWith('[') && rawOutput.endsWith(']')) {
            jsonItems = JSON.parse(rawOutput) as JsonDiagnostic[];
          } else {
            // Find JSON array substring if extra logs were printed
            const start = rawOutput.indexOf('[');
            const end = rawOutput.lastIndexOf(']');
            if (start !== -1 && end > start) {
              jsonItems = JSON.parse(rawOutput.substring(start, end + 1)) as JsonDiagnostic[];
            }
          }
        } catch {
          // If JSON parsing fails, do not crash
        }

        this.applyDiagnostics(document, jsonItems);
        this.updateStatusBar();
      }
    );
  }

  private applyDiagnostics(document: vscode.TextDocument, jsonItems: JsonDiagnostic[]): void {
    const diagnostics: KeetDiagnostic[] = [];

    for (const item of jsonItems) {
      const loc = item.location;
      const startLine = Math.max(0, (loc.line || 1) - 1);
      const startCol = Math.max(0, (loc.column || 1) - 1);
      let endLine = Math.max(0, (loc.endLine || loc.line || 1) - 1);
      let endCol = Math.max(0, (loc.endColumn || loc.column || 1) - 1);

      if (startLine === endLine && startCol === endCol) {
        if (loc.length && loc.length > 0) {
          endCol = startCol + loc.length;
        } else {
          // Highlight at least 1 character or word
          const lineText = document.lineAt(Math.min(startLine, document.lineCount - 1)).text;
          endCol = Math.min(lineText.length, startCol + 1);
          if (endCol <= startCol) {
            endCol = startCol + 1;
          }
        }
      }

      const range = new vscode.Range(startLine, startCol, endLine, endCol);

      let severity = vscode.DiagnosticSeverity.Error;
      if (item.severity === 'warning') {
        severity = vscode.DiagnosticSeverity.Warning;
      } else if (item.severity === 'note' || item.severity === 'help') {
        severity = vscode.DiagnosticSeverity.Information;
      }

      let message = item.message;
      if (item.helps && item.helps.length > 0) {
        message += '\n\n' + item.helps.map(h => `= help: ${h}`).join('\n');
      }
      if (item.notes && item.notes.length > 0) {
        message += '\n\n' + item.notes.map(n => `= note: ${n}`).join('\n');
      }

      const diag: KeetDiagnostic = new vscode.Diagnostic(range, message, severity);
      diag.code = item.code;
      diag.source = 'keety';
      diag.rawJson = item;

      if (item.labels && item.labels.length > 1) {
        const related: vscode.DiagnosticRelatedInformation[] = [];
        for (const lbl of item.labels) {
          if (!lbl.isPrimary) {
            const rStartLine = Math.max(0, (lbl.line || 1) - 1);
            const rStartCol = Math.max(0, (lbl.column || 1) - 1);
            const rEndLine = Math.max(0, (lbl.endLine || lbl.line || 1) - 1);
            const rEndCol = Math.max(0, (lbl.endColumn || lbl.column || 1) - 1);
            const rRange = new vscode.Range(rStartLine, rStartCol, rEndLine, rEndCol);
            related.push(new vscode.DiagnosticRelatedInformation(new vscode.Location(document.uri, rRange), lbl.message));
          }
        }
        diag.relatedInformation = related;
      }

      diagnostics.push(diag);
    }

    this.diagnosticCollection.set(document.uri, diagnostics);
  }

  public getDiagnostics(uri: vscode.Uri): readonly vscode.Diagnostic[] {
    return this.diagnosticCollection.get(uri) || [];
  }

  private updateStatusBar(): void {
    let errorCount = 0;
    let warningCount = 0;

    this.diagnosticCollection.forEach((_uri, diags) => {
      for (const d of diags) {
        if (d.severity === vscode.DiagnosticSeverity.Error) {
          errorCount++;
        } else if (d.severity === vscode.DiagnosticSeverity.Warning) {
          warningCount++;
        }
      }
    });

    if (errorCount === 0 && warningCount === 0) {
      this.statusBarItem.text = '$(check) Keet: OK';
      this.statusBarItem.backgroundColor = undefined;
    } else if (errorCount > 0) {
      this.statusBarItem.text = `$(error) Keet: ${errorCount} error${errorCount > 1 ? 's' : ''}`;
      this.statusBarItem.backgroundColor = new vscode.ThemeColor('statusBarItem.errorBackground');
    } else {
      this.statusBarItem.text = `$(warning) Keet: ${warningCount} warning${warningCount > 1 ? 's' : ''}`;
      this.statusBarItem.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
    }
  }

  public dispose(): void {
    for (const timer of this.debounceTimers.values()) {
      clearTimeout(timer);
    }
    this.debounceTimers.clear();
    this.diagnosticCollection.clear();
    this.diagnosticCollection.dispose();
    this.statusBarItem.dispose();
    for (const d of this.disposables) {
      d.dispose();
    }
  }
}
