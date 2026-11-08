# CyCodeIDE

IDE natif C++20 extrait de CyCode 0.2.3. **Version de développement** : aucune
nouvelle version n’est publiée par cette séparation.

![CyCode](icons/CyCode.png)

L’interface SDL/OpenGL/CyUI et ses panneaux sont ici. Le moteur, les documents,
projets, services sémantiques et transactions restent dans CyCode. L’éditeur
partagé de Reader conserve son API 1 ; ses utilitaires natifs viennent de CyCode.

```sh
python app/scripts/build.py --core-root <CyCode> --editor-root <CyCodeReader> --fetch
```

Voir le [guide de build](README.md#build) et le [bilan](Docs/Extraction.md).
La documentation CyDoc hors ligne reste générée depuis sa source commune dans
CyCode. Le build ne lance ni interface ni tests natifs. Les lanceurs de tests
locaux restent suspendus pendant le diagnostic antivirus. La distribution,
l’identité des mises à jour et la validation graphique feront l’objet d’un lot
suivant avant publication. Licence GPL-3.0-only.
