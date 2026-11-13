# MekoraLang para VS Code

Colorea los programas `.gc`: palabras reservadas, builtins, ejes, números,
cadenas y comentarios. `Ctrl+;` comenta y descomenta líneas con `//`, y las
llaves y paréntesis se cierran solos.

## Instalar

La forma más simple es copiar esta carpeta (no hace falta compilar nada) a
la carpeta de extensiones de VS Code, con este nombre:

    %USERPROFILE%\.vscode\extensions\mekoralang-ugd.mekoralang-0.1.0

y reiniciar VS Code. Para sacarla, se borra esa carpeta.

Otra forma, con Node instalado, es armar el paquete e instalarlo:

    npx @vscode/vsce package --allow-missing-repository --skip-license
    code --install-extension mekoralang-0.1.0.vsix
