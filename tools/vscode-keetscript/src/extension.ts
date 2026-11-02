import * as vscode from 'vscode';
import { DiagnosticManager } from './diagnostics';
import { KeetCodeActionProvider } from './codeActions';
import { KeetCompletionItemProvider } from './completions';
import { KeetHoverProvider } from './hover';
import { CommandManager } from './commands';

let diagnosticManager: DiagnosticManager | null = null;

export function activate(context: vscode.ExtensionContext): void {
  // 1. Live diagnostics manager & background worker
  diagnosticManager = new DiagnosticManager();
  context.subscriptions.push(diagnosticManager);

  // 2. Quick Fix Code Action Provider (lightbulb)
  context.subscriptions.push(
    vscode.languages.registerCodeActionsProvider(
      'keetscript',
      new KeetCodeActionProvider(),
      {
        providedCodeActionKinds: KeetCodeActionProvider.providedCodeActionKinds
      }
    )
  );

  // 3. IntelliSense Autocompletion Provider
  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      'keetscript',
      new KeetCompletionItemProvider(),
      '.',
      ':',
      '<'
    )
  );

  // 4. Hover Documentation Provider
  context.subscriptions.push(
    vscode.languages.registerHoverProvider(
      'keetscript',
      new KeetHoverProvider()
    )
  );

  // 5. Commands and Editor UI (Run ▶, Build, Check)
  const commandManager = new CommandManager(diagnosticManager);
  commandManager.registerCommands(context);
}

export function deactivate(): void {
  if (diagnosticManager) {
    diagnosticManager.dispose();
    diagnosticManager = null;
  }
}
