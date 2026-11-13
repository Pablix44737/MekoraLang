# MekoraLang para VS Code

Colorea los programas `.gc`: palabras reservadas, builtins, ejes, números,
cadenas y comentarios. `Ctrl+;` comenta y descomenta líneas con `//`, y las
llaves y paréntesis se cierran solos.

## Instalar

Hace falta Node. Con VS Code cerrado, desde esta carpeta, en PowerShell:

    npx.cmd @vscode/vsce package --allow-missing-repository --skip-license
    code --install-extension mekoralang-0.1.0.vsix

El primer comando arma el paquete `mekoralang-0.1.0.vsix` y el segundo lo
instala. Copiar la carpeta a mano en `.vscode\extensions` no alcanza: VS Code
no la registra. Si `code` no se reconoce, se puede instalar desde VS Code con
"Extensions: Install from VSIX...". El `.vsix` no se versiona.

Para sacarla, se desinstala desde el panel de extensiones.
