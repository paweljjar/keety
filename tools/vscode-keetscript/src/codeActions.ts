import * as vscode from 'vscode';
import { KeetDiagnostic, JsonDiagnosticSuggestion } from './diagnostics';

export class KeetCodeActionProvider implements vscode.CodeActionProvider {
  public static readonly providedCodeActionKinds = [
    vscode.CodeActionKind.QuickFix
  ];

  public provideCodeActions(
    document: vscode.TextDocument,
    _range: vscode.Range | vscode.Selection,
    context: vscode.CodeActionContext,
    _token: vscode.CancellationToken
  ): vscode.CodeAction[] {
    const actions: vscode.CodeAction[] = [];

    for (const diagnostic of context.diagnostics as KeetDiagnostic[]) {
      // 1. Process structured suggestions if provided by compiler
      if (diagnostic.rawJson && diagnostic.rawJson.suggestions && diagnostic.rawJson.suggestions.length > 0) {
        for (const sugg of diagnostic.rawJson.suggestions) {
          const action = this.createActionFromSuggestion(document, diagnostic, sugg);
          if (action) {
            actions.push(action);
          }
        }
      }

      // 2. Fuzzy typo suggestion parsing from message/helps
      const typoMatches = this.extractTypoSuggestions(diagnostic);
      for (const typo of typoMatches) {
        // Only add if not already covered by structured suggestion
        if (!actions.some(a => a.title.includes(`'${typo}'`))) {
          const action = new vscode.CodeAction(
            `Change to '${typo}'`,
            vscode.CodeActionKind.QuickFix
          );
          action.diagnostics = [diagnostic];
          action.isPreferred = true;
          action.edit = new vscode.WorkspaceEdit();
          action.edit.replace(document.uri, diagnostic.range, typo);
          actions.push(action);
        }
      }

      // 3. Missing import suggestion parsing: "add 'import <mod>;'"
      const importMatches = this.extractImportSuggestions(diagnostic);
      for (const mod of importMatches) {
        if (!actions.some(a => a.title.includes(`import ${mod};`))) {
          const action = new vscode.CodeAction(
            `Add 'import ${mod};' to file`,
            vscode.CodeActionKind.QuickFix
          );
          action.diagnostics = [diagnostic];
          action.isPreferred = true;
          action.edit = new vscode.WorkspaceEdit();

          // Find the best line to insert import
          const insertPosition = this.findImportInsertPosition(document);
          action.edit.insert(document.uri, insertPosition, `import ${mod};\n`);
          actions.push(action);
        }
      }

      // 4. Missing semicolon suggestion
      if (diagnostic.message.includes("add ';'") || diagnostic.message.includes("expected ';'")) {
        const line = document.lineAt(diagnostic.range.end.line);
        if (!line.text.trim().endsWith(';')) {
          const action = new vscode.CodeAction(
            "Add missing ';'",
            vscode.CodeActionKind.QuickFix
          );
          action.diagnostics = [diagnostic];
          action.edit = new vscode.WorkspaceEdit();
          action.edit.insert(document.uri, line.range.end, ';');
          actions.push(action);
        }
      }

      // 5. Assignment in condition: did you mean '=='?
      if (diagnostic.message.includes("did you mean '=='")) {
        const action = new vscode.CodeAction(
          "Replace '=' with '=='",
          vscode.CodeActionKind.QuickFix
        );
        action.diagnostics = [diagnostic];
        action.isPreferred = true;
        action.edit = new vscode.WorkspaceEdit();
        action.edit.replace(document.uri, diagnostic.range, '==');
        actions.push(action);
      }
    }

    return actions;
  }

  private createActionFromSuggestion(
    document: vscode.TextDocument,
    diagnostic: KeetDiagnostic,
    sugg: JsonDiagnosticSuggestion
  ): vscode.CodeAction | null {
    if (sugg.type === 'replace') {
      const startLine = Math.max(0, (sugg.startLine || 1) - 1);
      const startCol = Math.max(0, (sugg.startColumn || 1) - 1);
      const endLine = Math.max(0, (sugg.endLine || sugg.startLine || 1) - 1);
      const endCol = Math.max(0, (sugg.endColumn || sugg.startColumn || 1) - 1);
      const targetRange = new vscode.Range(startLine, startCol, endLine, endCol);

      const action = new vscode.CodeAction(
        sugg.title || `Change to '${sugg.replacement}'`,
        vscode.CodeActionKind.QuickFix
      );
      action.diagnostics = [diagnostic];
      action.isPreferred = true;
      action.edit = new vscode.WorkspaceEdit();
      action.edit.replace(document.uri, targetRange, sugg.replacement);
      return action;
    }

    if (sugg.type === 'insert') {
      const line = Math.max(0, (sugg.startLine || 1) - 1);
      const col = Math.max(0, (sugg.startColumn || 1) - 1);
      const pos = new vscode.Position(line, col);

      const action = new vscode.CodeAction(
        sugg.title || `Insert '${sugg.replacement}'`,
        vscode.CodeActionKind.QuickFix
      );
      action.diagnostics = [diagnostic];
      action.isPreferred = true;
      action.edit = new vscode.WorkspaceEdit();
      action.edit.insert(document.uri, pos, sugg.replacement);
      return action;
    }

    return null;
  }

  private extractTypoSuggestions(diagnostic: vscode.Diagnostic): string[] {
    const results: string[] = [];
    const text = diagnostic.message;
    const regex = /did you mean ['"]([a-zA-Z0-9_]+)['"]\?/g;
    let match: RegExpExecArray | null;
    while ((match = regex.exec(text)) !== null) {
      if (!results.includes(match[1])) {
        results.push(match[1]);
      }
    }
    return results;
  }

  private extractImportSuggestions(diagnostic: vscode.Diagnostic): string[] {
    const results: string[] = [];
    const text = diagnostic.message;
    const regex = /add ['"]import\s+([a-zA-Z0-9_]+);?['"]/g;
    let match: RegExpExecArray | null;
    while ((match = regex.exec(text)) !== null) {
      if (!results.includes(match[1])) {
        results.push(match[1]);
      }
    }
    return results;
  }

  private findImportInsertPosition(document: vscode.TextDocument): vscode.Position {
    let lastImportLine = -1;
    for (let i = 0; i < document.lineCount; i++) {
      const line = document.lineAt(i).text.trim();
      if (line.startsWith('import ') || line.startsWith('from ')) {
        lastImportLine = i;
      } else if (line.length > 0 && !line.startsWith('//') && !line.startsWith('/*')) {
        // Reached first non-comment, non-import code
        if (lastImportLine !== -1) {
          return new vscode.Position(lastImportLine + 1, 0);
        }
        return new vscode.Position(i, 0);
      }
    }
    if (lastImportLine !== -1) {
      return new vscode.Position(lastImportLine + 1, 0);
    }
    return new vscode.Position(0, 0);
  }
}
