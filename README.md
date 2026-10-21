# TuxDSK

Gestionnaire d'images disque (`.DSK`) pour Amstrad CPC, sous la forme d'un
plugin de [TuxAPE](../TuxAPE). C'est ManageDsk de Ludovic Deplanque
(Demoniak), version 0.20g, réécrit en C++ : mêmes fonctions, mêmes disques
produits, dans une fenêtre de TuxAPE et dans un programme en ligne de
commande.

## Ce qu'il fait

La fenêtre s'ouvre par le menu **Plugins ▸ TuxDSK – Disc Manager...**
(Ctrl+Maj+F6) ou par son bouton dans le panneau du bas. Elle montre le contenu
d'un disque, et d'un second à côté avec **Use two windows**.

| Bouton | Ce qu'il fait |
|---|---|
| Create DSK, Read DSK, Save DSK, Save As | un disque vierge (DATA ou VENDOR), lu d'un fichier `.DSK` ou `.HFE`, sauvé dans l'un ou l'autre format |
| Add File(s) | des fichiers du PC sur le disque : au choix pour chacun, forcés en ASCII ou forcés en binaire |
| Import from DSK | les fichiers d'un autre disque, lu d'un fichier `.DSK` ou `.HFE` : cochés dans la liste de ses fichiers, copiés tels quels avec leur user et leurs attributs |
| Export File(s) | des fichiers du disque vers le PC, avec ou sans leur en-tête AMSDOS |
| Rename File(s) | un autre nom, un autre numéro d'user |
| Delete File(s) | l'effacement, définitif ou non |
| Information | type, adresse de début, point d'entrée, lecture seule, caché : modifiables |
| Show File | le fichier en hexadécimal, en listing (BASIC, même protégé, ou texte), en source Dams, désassemblé, ou en écran |
| Show Sectors | les secteurs du disque, piste par piste |
| Copy =>, Copy <=, Swap, Compare | d'un disque à l'autre : copie de fichiers, échange, comparaison de deux fichiers ou des deux disques |
| Insert in A:, Insert in B:, Run | le disque dans un lecteur du CPC de TuxAPE, et le fichier choisi lancé |

Dans la fenêtre **Find Graphics** de TuxAPE, le menu du bouton droit reçoit
**Copy to TuxDSK's Disc...** tant que le plugin est chargé : la sélection va
dans un fichier du premier disque, dont le nom est demandé. S'il n'y a pas de
disque, il en est créé un.

- En mode **Sprite**, les sprites du Plus vont tels quels dans un `.SPR`
  (`SPRITE.SPR` pour commencer, puis `SPRITE1.SPR`...), avec un en-tête AMSDOS
  qui les fait recharger à l'adresse où ils étaient, sauf si on décoche la case.
- En mode **0, 1, 2** ou **Current**, la mémoire écran sélectionnée devient une
  fenêtre d'OCP Art Studio, un `.WIN` (`SCREEN.WIN`, `SCREEN1.WIN`...) : les
  tuiles côte à côte, comme Find Graphics les montre sur une rangée, ligne après
  ligne, puis les cinq octets qui donnent sa largeur en bits et sa hauteur,
  comme TuxASC et Martine les écrivent. Avec l'en-tête AMSDOS, elle se charge
  en &4000, comme les fenêtres d'OCP Art Studio. Une fenêtre fait au plus
  255 lignes de haut.

Les autres plugins peuvent aussi mettre un fichier sur le premier disque,
par la version 3 de l'interface des plugins de TuxAPE : TuxDSK leur offre
`putFile(nom, contenu, remplacer)`, qui crée un disque s'il n'y en a pas et
demande avant de remplacer un fichier du même nom, et `discName()`. C'est ainsi
que TuxCMP y met ses fichiers compressés et leur loader.

Un fichier effacé dont l'entrée est encore là apparaît en rouge, avec l'user
229 : **Rename** lui redonne un autre user, et le fichier revient. **Show
Directory** montre les 64 entrées du répertoire telles qu'elles sont. On peut
déposer des fichiers ou un `.DSK` sur la liste.

Le visualiseur choisit son mode d'après le fichier, comme ManageDsk : un
programme BASIC est listé, un `.SCR` ou un `.WIN` est une image (avec la
palette du `.PAL` de même nom s'il est sur le disque), un `.DAM` est un
source. En mode écran on règle le mode, la largeur, la hauteur, le décalage
et chaque encre ; une image « à plat » se sauve en fenêtre OCP (`.WIN`), tout
listing en texte, toute image en PNG.

Là où ManageDsk lit et écrit de vraies disquettes (lecteur A: du PC, pilote
`fdrawcmd.sys`), TuxDSK s'adresse aux lecteurs du CPC émulé : c'est le sens
de **Insert in A:** et **Insert in B:**.

## Vers un vrai CPC : le format HFE

**Save As** propose deux formats : `.DSK`, pour les émulateurs, et `.HFE`, le
format du HxC Floppy Emulator de Jean-François Del Nero, que lisent un Gotek
(firmware HxC ou FlashFloppy) et un lecteur HxC branchés à la place du
lecteur d'un vrai CPC. Le format suit le nom du fichier : un nom en `.hfe`
donne un fichier HFE, tout autre un DSK ; **Save DSK** garde le format du
fichier d'où vient le disque. **Read DSK** lit les deux.

Un fichier `.HFE` ne contient pas des secteurs mais chaque piste telle que
la tête la lit, bit par bit (MFM, 250 kbit/s), avec ses gaps, ses marques et
ses CRC. TuxDSK écrit ces pistes comme le fait le logiciel HxC lui-même :
pour un disque ordinaire (des pistes de neuf secteurs de 512 octets, comme
ceux que TuxDSK crée), le fichier est identique, octet pour octet, à celui
que produit son convertisseur `hxcfe` à partir du même `.DSK`
(`tests/golden/hfe_*`, refaits par `tools/hxc-goldens/run.sh`). Les écarts,
qui ne touchent que des pistes non formatées ou protégées, sont dans
[`notes/ecarts.md`](notes/ecarts.md). C'est
le premier format HFE (« HXCPICFE », révision 0), celui que tous les
firmwares acceptent.

## Le programme en ligne de commande

`tuxdsk` prend la ligne de commande de ManageDsk : des commandes qui se
suivent, chacune un `-`, une lettre et, sans espace, ce sur quoi elle porte.

| Commande | Ce qu'elle fait |
|---|---|
| `-C` | crée un disque vierge (DATA, 42 pistes) |
| `-L<disque>` | lit un fichier `.DSK` ou `.HFE` |
| `-A<masque>` | ajoute des fichiers tels quels : `-A*.BIN` |
| `-I<fichier>/<nom>/<type>/<début>/<entrée>` | ajoute un fichier sous un nom, avec l'en-tête de son type : `BIN`, `BAS` ou `ASCII` |
| `-S<disque>` | sauve le disque : en `.HFE` si le nom finit ainsi, en `.DSK` sinon |

```sh
tuxdsk -C -A*.BIN -Stoto.dsk
tuxdsk -LOld.dsk -A*.BAS -Iecran.raw/ECRAN.SCR/BIN/49152 -SNew.dsk
tuxdsk -LNew.dsk -SDSKA0000.HFE
tuxdsk -help
```

Il est dans l'archive du plugin, sous `bin/` : une fois le plugin installé,
`plugins/tuxdsk/bin/tuxdsk`. Il ne dépend pas de Qt.

## Fidélité à ManageDsk

Là où ManageDsk est juste, TuxDSK écrit les mêmes octets : les tests le
vérifient contre des disques écrits par le vrai ManageDsk (`tests/golden`),
lancé sous wine par `tools/managedsk-goldens/run.sh`. Là où ManageDsk a un
défaut (des octets perdus quand un disque grandit, une image étendue abîmée
à la sauvegarde, un enregistrement compté en trop...), TuxDSK fait ce que la
commande veut évidemment dire, et l'écart est noté dans
[`notes/ecarts.md`](notes/ecarts.md).

Une image relue puis réécrite sans changement est le fichier d'origine,
octet pour octet, quel que soit son contenu (pistes non formatées, secteurs
de tailles diverses, protections) : c'est vérifié sur tous les `.dsk` de la
bibliothèque de TuxAPE quand elle est là.

## Compiler

TuxDSK suit les conventions des plugins de TuxAPE (`PLUGINS.md`, à la racine
de TuxAPE) et ne prend de TuxAPE que son dossier `sdk/` : `plugin.h` et le
module CMake `TuxAPEPlugin.cmake`. TuxAPE doit se trouver à côté, dans
`../TuxAPE`, ou ailleurs avec `-DTUXAPE_SDK=/chemin/TuxAPE/sdk`. Il lui faut
la version 4 de l'interface des plugins : Find Graphics (version 2), la
langue de TuxAPE (version 4).

```sh
cmake --preset dev          # Qt du système, ou celui du .deps/ de TuxAPE
cmake --build build -j
ctest --preset dev
```

L'archive à installer est alors dans `dist/` :
`tuxdsk-<version>-<plateforme>.zip`. Elle contient le dossier du plugin :
`plugin.json`, la bibliothèque, `bin/tuxdsk` et `LICENSE`, et
`signature.json` quand la clé de l'auteur est nommée :

```sh
cmake -S . -B build -DTUXAPE_SIGN_KEY=$HOME/.config/tuxape-signing/moi.key
cmake --build build
```

Sans clé nommée, l'archive n'est pas signée : compiler TuxDSK ne le signe
jamais au nom de quelqu'un par mégarde.

## Installer

Dans TuxAPE : **Settings ▸ Plugins ▸ Install from ZIP...** (**Réglages ▸
Plug-ins** en français), choisir l'archive, confirmer. Le menu **Plugins**
reçoit « TuxDSK – Disc Manager... ». Installer une archive plus récente
remplace l'ancienne.

## Les langues

TuxDSK parle la langue de TuxAPE : anglais, la langue de ses sources, ou
français. Sa traduction est dans `translations/tuxdsk_fr.ts` ; elle reprend
les mots de la version française de ManageDsk. Elle est compilée dans la
bibliothèque et chargée au démarrage du plugin. Après un changement des
textes :

```sh
cmake --build build --target tuxdsk_update_translations   # lupdate
linguist translations/tuxdsk_fr.ts                         # traduire ce qui est nouveau
cmake --build build
```

Le moteur n'a pas de texte : il rend des codes (`ImageError`, `Difference`)
que les fenêtres disent dans la langue de TuxAPE, et le programme en ligne de
commande en anglais.

## Tests

```sh
ctest --preset dev
```

Les tests du moteur (`engine_disc_*`, `engine_view_*`) comparent ses sorties
à celles du vrai ManageDsk ; ceux de la ligne de commande (`cli_*`) lancent
le programme ; `plugin_window` charge le plugin comme TuxAPE le charge, en
anglais, en français et dans une langue qu'il n'a pas ; ceux de l'interface
(`gui_*`) pilotent les fenêtres sans écran.

`engine_disc_image` relit et réécrit aussi chaque `.dsk` d'un dossier de
disquettes à soi, s'il est nommé : `-DTUXDSK_DISC_DIR=/chemin/disquettes`.
Chacune doit redevenir le fichier qu'elle était.

`tools/managedsk-goldens/run.sh` refait les disques de référence : il lui
faut wine et `MFC42.DLL` (`winetricks mfc42`). `tools/hxc-goldens/run.sh`
refait les fichiers `.HFE` de référence avec `hxcfe`, compilé depuis les
sources du HxC Floppy Emulator.

## Organisation

Le projet est écrit en C++20. Le moteur ne dépend ni de Qt, ni de TuxAPE, ni
du système, n'écrit rien sur le disque (il rend des fichiers en mémoire) et
n'a aucun texte pour l'utilisateur.

| Dossier | Contenu |
|---|---|
| `src/engine/base` | version |
| `src/engine/disc` | images `.DSK` et `.HFE`, répertoire et fichiers AMSDOS, en-têtes, import et export, comparaison |
| `src/engine/view` | ce qu'on montre d'un fichier : hexadécimal, BASIC, Dams, Z80, écran, fichiers OCP |
| `src/plugin` | les fenêtres (`tuxdsk_ui`), le lien avec TuxAPE, la classe chargée (`tuxdsk`), tout dans `namespace tuxdsk` |
| `src/cli` | le programme `tuxdsk` |
| `translations` | la traduction française |
| `tests` | programmes de test et disques de référence |
| `tools` | ce qui refait les disques et les fichiers HFE de référence |
| `notes` | écarts voulus avec ManageDsk |
| `ManageDsk_V0.20g` | le ManageDsk d'origine, gardé ici comme référence (non versionné) |

## Licence

ManageDsk est un gratuiciel de Ludovic Deplanque (Demoniak). iDSK, publié sous
licence MIT, en contient des parties ; TuxDSK reprend d'iDSK les tables
d'instructions Z80 et est distribué sous la même licence (voir `LICENSE`).
