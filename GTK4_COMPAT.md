# GTK4 Visual Compatibility Checklist

Use this file to track known differences between the GTK2 and GTK4 versions of
`calfjackhost`. Test each item after a build and mark its status.

## How to test efficiently

```sh
# Show widget sizes and window measurements when opening any plugin:
calfjackhost --debug-layout monosynth

# Live CSS editing without rebuilding:
GTK_DEBUG=no-css-cache calfjackhost monosynth

# GTK Inspector (click any widget to see its CSS node, allocation, computed style):
GTK_DEBUG=interactive calfjackhost monosynth
# or press Ctrl+Shift+D at runtime
```

---

## Status legend

- ✅ Fixed and verified
- 🔲 Not yet tested
- ❌ Known broken
- ⚠️  Partial / minor difference remaining

---

## Rack view (main window)

| Widget | GTK2 appearance | Status | Commit | Notes |
|--------|----------------|--------|--------|-------|
| Rack background / side images | Side images tile vertically, corners at top/bottom | ✅ | c5caa607 | CSS `background-repeat: repeat-y` + vexpand spacer |
| Menu bar ("File", "Add plugin") | White text on dark background | ✅ | c5caa607 | `popovermenubar.Calf-Menu button { color }` CSS |
| Menu bar hover | Highlighted item | ✅ | c5caa607 | `popovermenubar.Calf-Menu button:hover` CSS |
| Plugin strip: knob indicator ring | Colored ring + tick visible | ✅ | c5caa607 | Hardcoded size-based metrics in `ctl_knob.cpp` |
| Plugin strip: × (close) button | Red × visible | ✅ | c5caa607 | CSS class `calf-button` added in init |
| Plugin strip: Open/Connect buttons | Dark text, pin indicator visible | ✅ | c5caa607 | CSS class + fixed pin draw coordinates |
| Plugin strip: MIDI/Audio labels | Colored labels | ✅ | 54a51105 | CSS `Calf-MidiLabel` / `Calf-AudioLabel` |
| Plugin strip: VU meter `-inf` label | White or themed color | 🔲 | — | Not yet investigated |
| Plugin strip: MIDI LED | Green dot | 🔲 | — | Appearance may differ |
| Plugin strip: waveform combo arrows | Down arrow visible | ✅ | c5caa607 | `dropdown.calf-combobox > button.arrow-button image { color }` |
| Rack title bar font | Themed color | ✅ | 0418acdf | CSS `Calf-Rack` color |

---

## Plugin window (opened via "Open" button)

| Widget | GTK2 appearance | Status | Commit | Notes |
|--------|----------------|--------|--------|-------|
| Window default size | Compact (~700 px wide for monosynth) | ✅ | 6390f6b3 | Was using natural width; switched to minimum |
| Rack ears (left/right side images) | Tile vertically, corners at extremes | ✅ | c5caa607 | Same fix as rack view |
| Menu bar ("Preset", "Help") | White text on dark background | ✅ | c5caa607 | Same CSS rule as rack menu bar |
| Knob indicator ring | Colored ring + tick visible | ✅ | c5caa607 | Same fix as rack view |
| Horizontal fader (hscale) | Themed slider image | 🔲 | — | Needs visual check |
| Vertical fader (vscale) | Themed slider image | 🔲 | — | Needs visual check |
| Line-graph (waveform preview) | ~150×88 px preview | ⚠️ | 6390f6b3 | ~168 px wide in default window due to frame expansion; acceptable |
| Line-graph (filter response) | ~130×100 px graph | ⚠️ | 6390f6b3 | Same as above |
| GtkNotebook tabs | Themed tabs with screw icon | 🔲 | — | Screw pixbuf rendering not checked |
| Dropdown / combo | Arrow visible, themed | ✅ | c5caa607 | Arrow color fixed |
| Toggle buttons (env-to-amp) | Themed toggle | 🔲 | — | Not yet checked |
| Label colors | White / themed | 🔲 | — | Not yet checked in plugin window context |

---

## CSS themes

All fixes are applied to all 8 themes via `gui/styles/*/gtk.css.in`.

| Theme | Tested |
|-------|--------|
| Calf_Default | 🔲 |
| Calf_0.0.19 | 🔲 |
| Calf_Flat_Default | 🔲 |
| Calf_Hybreed | 🔲 |
| Calf_Lost_Wages | 🔲 |
| Calf_Midnight | 🔲 |
| Calf_Orange | 🔲 |
| Calf_Wood | 🔲 |

---

## Known non-visual differences

| Area | GTK2 | GTK4 | Notes |
|------|------|------|-------|
| GtkTreeView (mod matrix) | Standard | Deprecated but functional | Use `G_GNUC_BEGIN_IGNORE_DEPRECATIONS` guard |
| `format-value` signal | Used for fader label | Removed in GTK4 | Fixed: `gtk_scale_set_format_value_func()` |
| GtkRC style properties | Used for knob metrics | Removed in GTK4 | Fixed: hardcoded lookup table in `ctl_knob.cpp` |
| `gtk_widget_size_request` | Returns minimum requisition | Removed in GTK4 | Fixed: use minimum slot of `gtk_widget_measure` |

---

## Debugging tips

### Isolate a size regression

```sh
calfjackhost --debug-layout monosynth
# prints:
# [debug-layout] plugin: Calf - Monosynth
# [debug-layout]   container  min=NxN  natural=NxN
# [debug-layout]   menubar    min=NxN
# [debug-layout]   window     default=NxN
```

If `natural` is much larger than `min`, a widget inside the container reports an
inflated natural size. Use GTK Inspector to identify which one.

### Identify a CSS problem

```sh
GTK_DEBUG=interactive calfjackhost monosynth
```

Click the problem widget → "CSS nodes" tab shows computed color, padding, etc.
Edit `gui/styles/Calf_Default/gtk.css` (the installed copy) live; run with
`GTK_DEBUG=no-css-cache` to pick up changes without rebuilding.
