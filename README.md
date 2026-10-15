# TuxDSK

Gestionnaire d'images disque (`.DSK`) pour Amstrad CPC, sous la forme d'un
plugin de [TuxAPE](../TuxAPE). C'est ManageDsk de Ludovic Deplanque
(Demoniak), version 0.20g, réécrit en C++ : mêmes fonctions, mêmes disques
produits, dans une fenêtre de TuxAPE et dans un programme en ligne de
commande.

## Ce qu'il fait

La fenêtre s'ouvre par le menu **Plugins ▸ TuxDSK – Disc Manager...**
(Ctrl+D) ou par son bouton dans le panneau du bas. Elle montre le contenu
d'un disque, et d'un second à côté avec **Use two windows**.

| Bouton | Ce qu'il fait |
|---|---|
| Create DSK, Read DSK, Save DSK, Save As | un disque vierge (DATA ou VENDOR), lu d'un fichier `.DSK` ou `.HFE`, sauvé dans l'un ou l'autre format |
| Add File(s) | des fichiers du PC sur le disque : au choix pour chacun, forcés en ASCII ou forcés en binaire |
| Export File(s) | des fichiers du disque vers le PC, avec ou sans leur en-tête AMSDOS |
| Rename File(s) | un autre nom, un autre numéro d'user |
| Delete File(s) | l'effacement, définitif ou non |
| Information | type, adresse de début, point d'entrée, lecture seule, caché : modifiables |
| Show File | le fichier en hexadécimal, en listing (BASIC, même protégé, ou texte), en source Dams, désassemblé, ou en écran |
| Show Sectors | les secteurs du disque, piste par piste |
| Copy =>, Copy <=, Swap, Compare | d'un disque à l'autre : copie de fichiers, échange, comparaison de deux fichiers ou des deux disques |
| Insert in A:, Insert in B:, Run | le disque dans un lecteur du CPC de TuxAPE, et le fichier choisi lancé |

Dans la fenêtre **Find Graphics** de TuxAPE, le menu du bouton droit reçoit
**Copy to TuxDSK's Disc...** tant que le plugin est chargé : les sprites ou les
tuiles sélectionnés vont dans un fichier du premier disque, dont le nom est
demandé (`SPRITE.SPR` pour commencer, puis `SPRITE1.SPR`...). S'il n'y a pas de
disque, il en est créé un. Le fichier reçoit un en-tête AMSDOS qui le fait
recharger à l'adresse où étaient les sprites, sauf si on décoche la case.

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

TuxAPE doit se trouver à côté, dans `../TuxAPE` (ou ailleurs avec
`-DTUXAPE_DIR=...`) : le plugin se compile contre son fichier
`src/app/plugin.h`, et avec le Qt 6 de TuxAPE. Il lui faut la version 2 de
l'interface des plugins, celle qui donne accès à Find Graphics.

```sh
cmake --preset dev
cmake --build build -j
```

L'archive à installer est alors dans `dist/` :
`tuxdsk-<version>-<plateforme>.zip`. Elle contient le dossier du plugin :
`plugin.json`, la bibliothèque et `bin/tuxdsk`.

## Installer

Dans TuxAPE : **Settings ▸ Plugins ▸ Install from ZIP...**, choisir l'archive,
confirmer. Le menu **Plugins** reçoit « TuxDSK – Disc Manager... ». Installer
une archive plus récente remplace l'ancienne.

À la main : décompresser l'archive dans le dossier `plugins` de TuxAPE, puis
relancer TuxAPE.

## Tests

```sh
ctest --preset dev
```

Les tests du moteur (`disc_*`, `view_*`) comparent ses sorties à celles du
vrai ManageDsk ; ceux de la ligne de commande (`cli_*`) lancent le programme ;
ceux de l'interface (`gui_*`, `plugin_window`) pilotent les fenêtres sans
écran. `tools/managedsk-goldens/run.sh` refait les disques de référence : il
lui faut wine et `MFC42.DLL` (`winetricks mfc42`). `tools/hxc-goldens/run.sh`
refait les fichiers `.HFE` de référence avec `hxcfe`, compilé depuis les
sources du HxC Floppy Emulator.

## Organisation

Le projet est écrit en C++20. Les bibliothèques du moteur ne dépendent ni de
Qt ni du système, et n'écrivent rien sur le disque : elles rendent des
fichiers en mémoire.

| Dossier | Contenu |
|---|---|
| `src/base` | version |
| `src/disc` | images `.DSK` et `.HFE`, répertoire et fichiers AMSDOS, en-têtes, import et export, comparaison |
| `src/view` | ce qu'on montre d'un fichier : hexadécimal, BASIC, Dams, Z80, écran, fichiers OCP |
| `src/plugin` | interface Qt et lien avec TuxAPE |
| `src/cli` | le programme `tuxdsk` |
| `tests` | programmes de test et disques de référence |
| `tools` | ce qui refait les disques et les fichiers HFE de référence |
| `notes` | écarts voulus avec ManageDsk |
| `ManageDsk_V0.20g` | le ManageDsk d'origine, gardé ici comme référence (non versionné) |

## Licence

ManageDsk est un gratuiciel de Ludovic Deplanque (Demoniak). iDSK, publié sous
licence MIT, en contient des parties ; TuxDSK reprend d'iDSK les tables
d'instructions Z80 et est distribué sous la même licence (voir `LICENSE`).
