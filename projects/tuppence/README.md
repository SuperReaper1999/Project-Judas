# Tuppence

An ordinary JudasJS arcade project with two physical machines. Open `tuppence.judasproj` or run its exported package. No money, purchases or network service.

## Controls

- **A/D**, left stick or arrow buttons: choose drop position.
- **Space**, controller South or **Drop / Insert 2p**: insert one real piece.
- **Autoplay**: watch ordinary scripted input drive the machine.
- **Escape / Modes / Menu**: switch machines, pause, resume, save, load, restart or quit.

**Ball Pusher** starts with 112 brass balls. The centre pocket pays double. Reach 500 points, then continue playing.

**2p Coin Pusher** is a compact tabletop cabinet with 24 flat copper tokens, a reciprocating upper shelf and lower payout edge. Insert coins onto the shelf; native contact pushes coins over the front to return them. Every returned token pays 2p in virtual score and restores one token credit. Reach 100p, then continue. The native slider motor has finite force: a blocked shelf can slow down. Tokens use original embossed render meshes and 12-sided convex physical geometry, not spheres. This is an arcade approximation, not a dimensionally exact commercial cabinet or gambling simulator.

All pieces remain physical until they actually leave the collection edge or escape the cabinet. Ball capacity is 208; coin capacity is 32, and occupied/full drops are refused without spending a credit. No frozen background pieces or decorative population. Dense thin convex stacks remain expensive; see the measured limits in `Review/REPORT.md`.

Saving uses the normal Judas save service (one shared Tuppence slot). Loading restores the saved machine, physical bodies and ledger. Switching modes starts a fresh round; session best scores are separate. Saves are local user data, not source content.

## Authoring

`Scenes/cabinet.judas` and `Scenes/coins.judas` are editable scenes; `Scenes/crowded.judas` is a 208-ball stress fixture. `Assets/scripts/game.js` owns credits, collection, UI, machine policy and scene switching. `tools/CreateBallPusher.py` regenerates original cabinet content. `ArtSource/create_coin.py` and `coin.blend` preserve the coin source; Blender exports glTF and Judas's normal hull cooker creates the collision asset. Font licensing is alongside the font.

The package is exported with `build/judas_export projects/tuppence/tuppence.judasproj .cache/Tuppence-Package build/judas`. Run `.cache/Tuppence-Package/judas` from any working directory. Windows and human gameplay acceptance remain pending.

## Human checklist

1. Drop balls manually and watch the shelf push them into the collection tray.
2. Check regular/centre payout and credits; try autoplay.
3. Pause, resume, save, load and start a new machine.
4. Switch to 2p mode; insert actual flat coins and watch shelf transfer/payout.
5. Check both modes remain usable as the bed becomes crowded; report hitches.
6. Repeat in the exported package.
