# Prompter

A teleprompter for the [Elgato Prompter](https://www.elgato.com/prompter) on
Linux. Elgato only ships Camera Hub for Windows and macOS; this drives the
Prompter's screen directly.

## How it works

The Prompter is a DisplayLink device (`17e9:ff1a`, DL-3xxx). DisplayLink's
`DisplayLinkManager` exposes it as an `evdi` DRM card. Hyprland can't drive
evdi outputs on NVIDIA (NVIDIA won't share buffers with evdi; see
[aquamarine#279](https://github.com/hyprwm/aquamarine/pull/279)), so instead
Hyprland is kept off that card (a udev rule puts it on its own seat) and Prompter becomes its DRM master, rendering
frames on the CPU and page-flipping them onto the panel. The glass only ever
shows the script: no stray cursor, notifications or windows.

## Setup

```sh
yay -S evdi-dkms displaylink
sudo systemctl enable --now displaylink.service
```

Move the Prompter's DRM card off your seat so Hyprland ignores it, and hand it
to your user ([`system/72-elgato-prompter.rules`](system/72-elgato-prompter.rules)),
then log out and back in:

```sh
sed "s/OWNER=\"tevans\"/OWNER=\"$USER\"/" system/72-elgato-prompter.rules |
  sudo tee /etc/udev/rules.d/72-elgato-prompter.rules
sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=drm
```

Then `bin/install` builds and installs the package.

The glass is not mirrored by default: the Elgato Prompter's own optics already
present the panel the right way round. `M` flips it if yours differs.

## Using it

Type or paste a script, or open a `.txt`/`.md` file (`prompter talk.md`, or
drop it on the window). Markdown headings and emphasis work, every line break
is kept, and `[stage directions]` are highlighted in the theme's accent.
The script is saved as a draft automatically; opened files are never changed.

| Key | Action |
| --- | --- |
| Space / Page Down | Roll or pause (clicker forward) |
| Ctrl+Space | Roll or pause, even while editing |
| Page Up | Back three lines (clicker back) |
| ↑ / ↓ | Nudge a line back or forward |
| ← / → | Slower / faster |
| Home | Back to the top |
| + / − | Bigger / smaller text |
| [ / ] | Narrower / wider margins |
| M | Mirror the glass on or off |
| E / Enter | Edit the script |
| Esc | Leave the editor |
| Scroll on preview | Scrub through the script |
| ? | Shortcut help |
| Q | Quit |

## Development

`bin/build`, `bin/test`, `bin/install`.
