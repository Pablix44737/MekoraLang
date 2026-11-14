# MekoraLang para VS Code

Colorea los programas `.gc`: palabras reservadas, builtins, ejes, números,
cadenas y comentarios. `Ctrl+;` comenta y descomenta líneas con `//`, y las
llaves y paréntesis se cierran solos.

## Instalar

Desde la raíz del proyecto:

    code --install-extension vscode-mekoralang/mekoralang-0.1.0.vsix

También se puede instalar desde VS Code con "Extensions: Install from
VSIX...". Copiar la carpeta a mano en `~/.vscode/extensions` no alcanza.

Si se cambia la gramática de colores, el paquete se vuelve a armar desde esta
carpeta con Node:

    npx @vscode/vsce package --allow-missing-repository --skip-license

Para sacarla, se desinstala desde el panel de extensiones.
