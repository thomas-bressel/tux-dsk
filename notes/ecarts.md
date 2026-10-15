# Écarts entre TuxDSK et ManageDsk

Ce document recense tout ce que TuxDSK ne fait pas comme ManageDsk 0.20g.

La règle suivie : quand ce que ManageDsk écrit est valide, TuxDSK écrit les mêmes octets, et les tests le vérifient contre des disques produits par ManageDsk lui-même (`tests/golden`, voir `tools/managedsk-goldens/run.sh`). Quand ce que ManageDsk écrit est manifestement faux, TuxDSK écrit ce dont un CPC a besoin et l'écart est noté ici. Dans le doute, les octets de ManageDsk sont gardés et le doute est noté dans la seconde partie.

Chaque entrée dit ce que fait ManageDsk, ce que fait TuxDSK, et pourquoi. « Vérifié » signifie que le comportement de ManageDsk a été observé en lançant `ManageDsk.exe` sous wine, par sa ligne de commande. Le reste vient de sa documentation, des textes de son exécutable et des sources d'iDSK, qui contient des parties des siennes : sa fenêtre n'a pas pu être pilotée.

Ce qui a été vérifié identique, octet pour octet : le disque vierge (`-C`), les fichiers ajoutés tels quels (`-A`) du plus court au plus long, les noms donnés aux fichiers, le fichier ajouté deux fois, l'en-tête AMSDOS fabriqué par `-I`, et la relecture suivie d'une sauvegarde d'une image « MV - CPCEMU ».

## 1. Écarts

### 1.1 Images disque

**E1. Sauvegarde d'une image étendue**
- ManageDsk : écrit toute image comme si chaque piste tenait `&1300` octets (9 secteurs de 512). Une image « EXTENDED CPC DSK » dont les pistes sont autrement faites est tronquée ou allongée : 438 528 octets en deviennent 399 104 pour un disque de 82 pistes, une image à deux faces de 429 312 octets en devient 194 816 (vérifié). Il écrit aussi `&13` à l'octet `&33` de l'en-tête, qu'une image étendue n'utilise pas (vérifié).
- TuxDSK : une image relue puis réécrite sans changement est le fichier d'origine, octet pour octet, quels que soient ses pistes, ses secteurs et ce qui les suit (`DiscImage::bytes()`). Vérifié sur les 5 908 images de la bibliothèque de TuxAPE.
- Pourquoi : ouvrir un disque protégé pour en regarder les secteurs puis le sauver ne doit pas le détruire.

**E2. Nombre de pistes à la lecture**
- ManageDsk : d'après les sources d'iDSK, ramène à 42 le nombre de pistes d'une image qui en a plus.
- TuxDSK : garde toutes les pistes.
- Pourquoi : les pistes au-delà de la 42e seraient perdues à la sauvegarde.

**E3. Disque qui grandit**
- ManageDsk : quand un fichier dépasse la dernière piste, ajoute les pistes une à une, mais seulement pour le premier des deux secteurs d'un bloc. Un bloc à cheval sur deux pistes perd sa seconde moitié : elle est recouverte de `&E5` quand la piste suivante est formatée, ou n'est pas sauvée du tout si c'était la dernière. Sur 224 Ko ajoutés à un disque vierge, 4 secteurs de 512 octets sont ainsi perdus (vérifié) ; sur 192 Ko, la fin du dernier fichier (vérifié, `tests/golden/grown.dsk`).
- TuxDSK : ajoute toutes les pistes dont le bloc a besoin avant de l'écrire (`CpcDisc::writeBlock()`). Le reste du disque est celui de ManageDsk, à l'octet près.
- Pourquoi : les fichiers doivent être entiers.

**E4. Disque à deux faces**
- ManageDsk : refuse une image à deux faces (« File is not recognized as standard DSK »).
- TuxDSK : l'ouvre ; les fichiers montrés sont ceux de la première face, les secteurs des deux faces se regardent, et la sauvegarde garde les deux.
- Pourquoi : on peut vouloir en extraire les fichiers.

### 1.2 Fichiers sur le disque

**E5. Octets après la fin d'un fichier**
- ManageDsk : écrit des blocs entiers pris dans un tampon qu'il ne vide pas. Après un fichier plus court que le précédent, la fin du dernier bloc contient des octets du fichier précédent (vérifié).
- TuxDSK : complète le dernier bloc avec des zéros, comme ManageDsk le fait pour le premier fichier.
- Pourquoi : le contenu d'un disque ne doit pas dépendre de l'ordre dans lequel on l'a rempli, ni contenir des restes d'un autre fichier.

**E6. Fichier vide**
- ManageDsk : n'écrit rien du tout pour un fichier de 0 octet, sans le dire (vérifié).
- TuxDSK : crée une entrée de répertoire sans enregistrement, comme AMSDOS.
- Pourquoi : le fichier demandé doit être sur le disque.

**E7. Disque plein**
- ManageDsk : d'après les sources d'iDSK, s'arrête au milieu du fichier quand la place ou les entrées manquent ; ce qui en est déjà écrit reste sur le disque.
- TuxDSK : vérifie avant d'écrire que le fichier tient ; sinon le disque reste comme il était (`CpcDisc::add()`).
- Pourquoi : un fichier tronqué sur le disque est pire que pas de fichier.

**E8. Caractères d'un nom**
- ManageDsk : garde tout caractère du nom du fichier.
- TuxDSK : retire ce qu'un catalogue de CPC ne peut pas contenir : espaces, signes réservés de CP/M (`<>.,;:=?*[]`) et tout ce qui n'est pas de l'ASCII imprimable (`cpcName()`). Pour un nom fait de lettres, de chiffres et de signes admis, le résultat est celui de ManageDsk, y compris sa règle du premier point : `my.file.name.txt` devient `MY.FIL` (vérifié sur 7 noms).
- Pourquoi : `MON FICHIER.BAS` ne peut être ni tapé ni chargé sur un CPC.

**E9. Fichier remplacé, type changé**
- ManageDsk : comportement non observé.
- TuxDSK : un fichier dont le contenu change sans changer de longueur reste dans ses blocs ; s'il s'allonge ou raccourcit (en-tête ajouté ou retiré par « Information »), il est retiré puis remis, et reste tel quel si le nouveau ne tient pas (`CpcDisc::replace()`).

### 1.3 Ligne de commande

**E10. `-I` : un enregistrement de trop**
- ManageDsk : compte 128 octets de trop pour tout fichier ajouté par `-I` : un fichier de 428 octets occupe 5 enregistrements au lieu de 4 (vérifié). En `ASCII`, ces 128 octets contiennent une seconde copie du début du fichier (vérifié).
- TuxDSK : compte les enregistrements du fichier. L'en-tête et les données sont ceux de ManageDsk, à l'octet près.
- Pourquoi : un fichier ASCII de 18 octets ne doit pas se lire comme 146.

**E11. `-I` sur un fichier qui a déjà un en-tête**
- ManageDsk : en `BIN`, met le nouvel en-tête devant l'ancien, qui devient le début des données ; en `ASCII`, garde l'en-tête dans le fichier (vérifié).
- TuxDSK : remplace l'en-tête en `BIN` et `BAS`, le retire en `ASCII`, comme la fenêtre de ManageDsk le fait en « Force ASCII ».
- Pourquoi : l'en-tête demandé doit être celui du fichier.

**E12. `-I` incomplet ou mal écrit**
- ManageDsk : sans adresses, écrit un en-tête fait de restes du fichier précédent, ou 128 zéros (vérifié). Ne reconnaît les types qu'en capitales : `bin` donne un fichier BASIC (vérifié). Prend un type inconnu pour du BASIC (vérifié). Lit `&8000` comme 0 (vérifié). Ne coupe pas le nom à huit caractères : `toolongname.binx` remplit nom et extension de `TOOLONGNAME` (vérifié).
- TuxDSK : les adresses absentes valent 0, et 368 (`&170`) pour le début d'un programme BASIC ; les types s'écrivent en capitales ou non ; un type inconnu est une erreur ; les adresses s'écrivent en décimal, ou en hexadécimal après `&`, `#` ou `0x` ; le nom suit la règle de tous les noms.
- Pourquoi : c'est ce que la documentation de ManageDsk annonce.

**E13. Chemins avec `/`**
- ManageDsk : sépare les champs de `-I` par `/`, les dossiers de Windows s'écrivant avec `\`.
- TuxDSK : cherche le type en partant de la fin, de sorte que `-I/home/moi/ecran.raw/ECRAN.SCR/BIN/49152` se lise comme on l'attend (`parseImport()`).

**E14. `*.BIN` et les noms longs**
- ManageDsk : sous Windows, `-A*.bin` trouve aussi `verylongfilename.binary`, par son nom court `VERYLO~1.BIN` (vérifié).
- TuxDSK : le masque porte sur le nom du fichier, sans distinguer les capitales.

**E15. Erreurs**
- ManageDsk : continue après une erreur, et rend toujours la main sans rien signaler au script qui l'a lancé. Un fichier introuvable passe inaperçu.
- TuxDSK : s'arrête à la première commande qui échoue, le dit sur la sortie d'erreur et rend le code 1 (2 si la ligne est mal écrite). Un fichier déjà présent sur le disque n'est pas ajouté et n'est pas une erreur, comme chez ManageDsk (vérifié) ; il est signalé.
- Pourquoi : un script de construction doit savoir que son disque est incomplet.

**E16. Sans argument, messages**
- ManageDsk : sans argument, ouvre sa fenêtre ; ses messages sont en français.
- TuxDSK : sans argument, `tuxdsk` affiche son aide ; ses messages sont en anglais, comme ceux de TuxAPE.

### 1.4 Visualiseur

**E17. Désassembleur**
- ManageDsk (d'après iDSK) : n'avance que d'un octet pour une instruction à deux opérandes d'un octet (`LD (IX+nn),nn`), de sorte que la suivante est lue un octet trop tôt ; écrit le déplacement des instructions `FD CB` sans signe et celui des `DD CB` avec.
- TuxDSK : avance de tous les octets de l'instruction, et écrit `(IX-02)` et `(IY-02)` de la même façon. Les noms des instructions sont ceux de ManageDsk (`src/view/z80tables.inc`).

**E18. Listing BASIC**
- ManageDsk (d'après iDSK) : écrit un nombre binaire en hexadécimal après `&X` ; traite comme des jetons le texte qui suit `REM`, `'` ou `DATA` ; décode un programme protégé par une table de 128 octets, juste pour les 128 premiers seulement ; affiche les nombres à virgule avec six décimales.
- TuxDSK : écrit les chiffres binaires ; laisse tel quel ce qui suit `REM` et `'`, et ce qui suit `DATA` jusqu'à la fin de l'instruction ; décode par les deux tables d'AMSDOS, de 13 et 11 octets, sur toute la longueur ; affiche neuf chiffres significatifs, comme le CPC.

**E19. Lettres accentuées**
- ManageDsk : convertit toujours `@ \ { | }` en `à ç é ù è` dans un listing.
- TuxDSK : seulement si la case « French accented letters » est cochée.
- Pourquoi : sur un CPC anglais, `|` est le signe des commandes RSX et `@` celui des pointeurs.

**E20. Source Dams**
- ManageDsk 0.15 et suivants : ajoute un `_` aux étiquettes qui sont des mots réservés (`READ`, `WRITE`, `END`).
- TuxDSK : ne le fait pas ; la règle exacte n'a pas pu être observée. Une étiquette suivie d'un seul commentaire le reçoit après un `;`, que la version d'iDSK oublie.

**E21. Fichier sans en-tête**
- ManageDsk (d'après iDSK) : montre et copie un fichier sans en-tête par blocs entiers de 1 Ko.
- TuxDSK : par enregistrements de 128 octets, comme le répertoire les compte.

### 1.5 Fenêtre

**E22. Disquettes réelles**
- ManageDsk : lit et écrit de vraies disquettes par le pilote `fdrawcmd.sys` de Windows, les formate, et propose des réglages du lecteur.
- TuxDSK : n'en fait rien. À la place, **Insert in A:** et **Insert in B:** mettent le disque dans les lecteurs du CPC de TuxAPE, et **Run** lance le fichier choisi.
- Pourquoi : le plugin vit dans un émulateur, dont les lecteurs sont ceux qu'on a sous la main.

**E23. Export de plusieurs fichiers**
- ManageDsk : demande un nom pour chaque fichier exporté.
- TuxDSK : demande un nom pour un seul fichier, un dossier pour plusieurs, qui y vont sous leur nom ; un fichier déjà là est signalé.

**E24. Ce que TuxDSK ajoute**
- Les colonnes « Start » et « Entry » de la liste ; le format VENDOR pour un disque créé ; l'enregistrement d'une image en PNG ; le format du disque et son nombre de pistes dans le titre.
- La lecture et l'écriture des fichiers `.HFE` du HxC Floppy Emulator, dans la fenêtre comme en ligne de commande (`-S` avec un nom en `.hfe`, `-L`) : voir la section 3.
- L'entrée **Copy to TuxDSK's Disc...** du menu de Find Graphics, dans TuxAPE : les sprites ou tuiles sélectionnés deviennent un fichier du premier disque, créé s'il n'existe pas.

**E25. Langue**
- ManageDsk : français, anglais ou espagnol.
- TuxDSK : anglais, comme TuxAPE et ses autres plugins ; les libellés sont ceux de la version anglaise de ManageDsk.

## 2. Doutes

**D1. Fin d'un fichier ASCII**
ManageDsk complète le dernier enregistrement d'un fichier sans en-tête avec des zéros (vérifié), là où AMSDOS met la marque de fin de fichier `&1A`. TuxDSK garde les zéros : les disques sont ceux de ManageDsk. Un programme qui lit le fichier caractère par caractère sur le CPC verra ces zéros après le texte ; si cela gêne, il faut terminer le fichier par `&1A` avant de l'ajouter.

**D2. Kilo-octets libres**
Le titre de ManageDsk donne « Ko utilisés/libres » ; le second nombre n'a pas pu être observé. TuxDSK compte les blocs libres sur les pistes que le disque a maintenant : 178 pour 40 pistes au format DATA, 187 pour 42. Un fichier peut encore y entrer quand le compte est à zéro, puisque le disque grandit jusqu'au bloc 188 (ou 255 avec « Allow more than 42 tracks »).

**D3. Limite à 42 pistes**
La ligne de commande de ManageDsk écrit jusqu'au bloc 255 (vérifié). Sa fenêtre, d'après sa documentation, s'arrête au bloc 188 sauf si « Permettre plus de 42 pistes » est coché ; TuxDSK fait de même, quel que soit le format, sans avoir pu le vérifier.

**D4. Changement de type BASIC / BASIC protégé**
« Information » change le type inscrit dans l'en-tête. TuxDSK ne chiffre ni ne déchiffre le programme quand on passe de l'un à l'autre : un programme protégé déclaré non protégé ne se chargera pas. Ce que fait ManageDsk n'a pas pu être observé ; le plus simple a été retenu.

**D5. Couleur 24 dans une palette**
La table de ManageDsk donne `&49` pour la couleur 24 (jaune vif), que le Gate Array appelle `&4A` ; `&49` est un double de `&43` (jaune pastel). TuxDSK lit `&4A` comme 24 et `&49` comme 25, ce qui est la table du CPC.

**D6. Nettoyage des en-têtes**
Les octets mis à zéro par « Auto clean-up » sont ceux que les sources d'iDSK désignent : 28 à 63 et 69 à 127. La somme de contrôle est recalculée sur ses deux octets (iDSK n'en écrit qu'un).

## 3. Fichiers HFE

TuxDSK écrit un `.HFE` comme le convertisseur `hxcfe` du logiciel HxC (version 2.16.19.1) l'écrit à partir du même `.DSK`. « Vérifié » signifie ici : comparé octet pour octet au fichier produit par `hxcfe`.

Ce qui est identique (vérifié) : l'en-tête, la liste des pistes, et toute piste formatée dont les secteurs ont dans l'image la longueur que leur ID annonce : disposition IBM (gap 4a de 80 octets, marque d'index, gap 1 de 50, puis pour chaque secteur ID, gap 2 de 22, données, gap 3 de l'en-tête de piste), longueur de la piste (6254 octets pour neuf secteurs de 512), données marquées effacées (`&F8`), CRC volontairement faux dans l'ID ou dans les données, piste formatée sans secteur, seconde face, remplissage du dernier bloc. Sur 258 disques de la bibliothèque de TuxAPE pris à intervalles réguliers, 201 donnent le même fichier ; 42 ne diffèrent que par des pistes jamais formatées (H1) et 15 que par des pistes de protection (H2).

**H1. Piste jamais formatée**
- hxcfe : y met 100 000 cellules tirées au hasard, différentes à chaque conversion.
- TuxDSK : y met un tour d'octets de gap (`&4E`), sans aucune marque : le même fichier à chaque fois, et rien qu'un contrôleur puisse y trouver non plus.

**H2. Secteur dont l'image a moins d'octets que son ID n'en annonce**
- hxcfe : lit dans le fichier `.DSK` autant d'octets que l'ID en annonce, donc au-delà du secteur : les secteurs suivants, puis l'en-tête de la piste suivante (« Track-Info... »).
- TuxDSK : continue de même avec ce qui suit sur la piste, puis, arrivé au bout de la piste, avec l'octet de remplissage.
- Cela ne concerne que des pistes de protection (secteurs de 8 Ko, secteurs imbriqués). Ces protections ne sont de toute façon rendues qu'à peu près par un `.HFE` de ce premier format, qui ne garde pas les bits faibles.

**H3. Numéros de piste de l'en-tête**
- hxcfe : range chaque piste à la place que dit le numéro de son en-tête dans le `.DSK`.
- TuxDSK : les prend dans l'ordre du fichier, comme les émulateurs. Les deux ne diffèrent que pour de rares images dont les en-têtes sont mal numérotés.

**H4. Taille écrite dans l'ID**
- hxcfe : écrit dans l'ID la taille qu'il a retenue pour le secteur, de 0 à 7.
- TuxDSK : écrit l'octet de taille du `.DSK` tel quel (une protection peut y mettre `&FF`).

**H5. Lecture**
À la lecture d'un `.HFE`, TuxDSK retrouve les secteurs par leurs marques, avec leurs erreurs de CRC et leurs données effacées. Le disque obtenu est une image étendue : sauvé en `.DSK`, il devient un « EXTENDED CPC DSK ». Seul le premier format HFE, en MFM, est lu ; pas le HFE v3 (« HXCHFEV3 »).
