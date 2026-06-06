# GTK4 Migration Plan

## Goal

Migrate this CherryTree fork from the current GTK3/gtkmm3/GtkSourceView4 Windows development path to a working GTK4/gtkmm4/GtkSourceView5 path, primarily so we can use `GtkSourceVimIMContext` for built-in Vim-style editing.

Release packaging is still out of scope for this plan, but GTK4 development builds must be deployed into a portable runtime directory before smoke testing. The first target is a local development build that starts from that portable package, opens documents, edits safely, and enables Vim mode.

## Current State

- The upstream stable CherryTree version is already 1.7.0, and this tree is based on 1.7.0.
- The project already has `WITH_GTK4` in `CMakeLists.txt`, plus `-Gtk4` in `build.ps1` and `gtk4` in `build.sh`.
- The GTK4 path is explicitly marked work in progress:
  - `CMakeLists.txt` has `option(WITH_GTK4 "Build with GTK4 (Work in progress...)" OFF)`.
- The current Windows dev environment has GTK3 dependencies installed:
  - `gtkmm3 3.24.10`
  - `gtksourceview4 4.8.4`
- The GTK4 dependencies are now installed in MSYS2 UCRT64:
  - `mingw-w64-ucrt-x86_64-gtkmm-4.0`
  - `mingw-w64-ucrt-x86_64-gtksourceview5`
  - `mingw-w64-ucrt-x86_64-libspelling`
- The codebase already contains many GTK4 conditional sections. Some are real implementations, some are temporary fallbacks.

## Progress Notes - 2026-06-06

The first GTK4 compile target is now reached on MSYS2 UCRT64.

Installed and verified dependency versions:

- `gtkmm-4.0` 4.18.0
- `gtksourceview-5` 5.16.0
- `glibmm-2.68` 2.84.0
- `pangomm-2.48` 2.56.1
- `libspelling-1` 0.4.8
- `libzmq` 4.3.5

Build verification completed:

- `.\build.ps1 -Gtk4` passes and leaves `build/cherrytree.exe` built with `WITH_GTK4=ON`.
- `.\build.ps1 -Gtk4 -Package -PackageArchiveFormat none` passes and deploys the executable into `build/cherrytree_1.7.0.0_win64_portable_gtk4_nolatex/ucrt64/bin/cherrytree.exe`.
- `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes and only refreshes the executable inside the existing portable package when runtime resources have not changed.
- `.\build.ps1` still passes for the GTK3 path.
- A hidden GTK4 startup smoke test from the portable package stayed running for 5 seconds before being stopped, with PATH reduced to Windows system directories, so the app does not rely on the MSYS2 PATH to locate runtime DLLs.
- A portable GTK4 `cherrytree.exe --version` smoke test prints `CherryTree 1.7.0` from `build/cherrytree_1.7.0.0_win64_portable_gtk4_nolatex/ucrt64/bin`.
- `ctest --test-dir build --output-on-failure` reports that no tests are registered.
- `git diff --check` passes.
- After the column-edit/dialog/tag-style batch, `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes again and updates the GTK4 portable executable in `build/cherrytree_1.7.0.0_win64_portable_gtk4_nolatex/ucrt64/bin`.
- The GTK4 portable executable still prints `CherryTree 1.7.0` with `--version`.
- After that same batch, `.\build.ps1` still passes for the GTK3 path; note that this leaves `build/cherrytree.exe` as a GTK3 executable while the GTK4 portable executable remains in the GTK4 portable directory.
- After the context-menu/IME batch, `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes again and refreshes the GTK4 portable executable.
- That refreshed GTK4 portable executable still prints `CherryTree 1.7.0`; `git diff --check` passes; `ctest --test-dir build --output-on-failure` still reports that no tests are registered.
- After the popup action-group batch, `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes again, rebuilding only the menu/main-window objects and refreshing the GTK4 portable executable.
- The refreshed GTK4 portable executable still prints `CherryTree 1.7.0`, `git diff --check` passes, and a 5-second portable startup smoke test stays running.
- After replacing GTK4 right-click popup menus with direct `Gtk::Popover`/`Gtk::Button` menus, `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes again and refreshes the GTK4 portable executable.
- The direct-popup build still prints `CherryTree 1.7.0`, `git diff --check` passes, `ctest` reports no registered tests, and a 5-second portable startup smoke test stays running.
- After the command-palette/Vim-remember batch, `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes again and refreshes the GTK4 portable executable.
- That build still prints `CherryTree 1.7.0`, `git diff --check` passes, `ctest` reports no registered tests, and a 5-second portable startup smoke test stays running.
- After the GTK4 IME/Vim persistence correction, `.\build.ps1 -Gtk4 -PackageFast -PackageArchiveFormat none` passes again and refreshes the GTK4 portable executable.
- That build still prints `CherryTree 1.7.0`, `git diff --check` passes, and `ctest` reports no registered tests. The startup smoke test was skipped because an existing CherryTree process from an external portable package was already running and the new process exited through the single-instance path.

Source changes made for the first GTK4 build:

- Guarded GTK3-only button image APIs in the start dialog.
- Guarded the unsafe-LaTeX warning icon fallback, matching the existing GTK4 empty-pixbuf fallback used elsewhere in that function.
- Updated `CtTreeView` tooltip wiring for gtkmm4's signal connection signature and GTK4's `gtk_tree_view_get_tooltip_context()` argument form.
- Connected the existing `vim_mode_toggle` menu action to a GTK4/GtkSourceView 5.4+ `GtkSourceVimIMContext` switch on the main `CtTextView`.
- Persisted the Vim toggle through `enable_vim_mode` in `[editor]`.
- Displayed Vim command/command-bar text in the status bar while Vim mode is active.
- Connected GtkSourceView's Vim `write` signal so plain `:w` uses CherryTree's normal save action.
- Restored the command palette dialog in GTK4 with search, filtering, keyboard navigation, and command selection.
- Restored the GTK4 import parent-node selection dialog instead of silently choosing the current node/root.
- Restored GTK4 sibling sorting for full-tree sort operations.
- Restored GTK4 codebox settings and table settings dialogs instead of returning cancel from anchored-widget fallbacks.
- Restored the GTK4 LaTeX text handling dialog, including syntax-highlighted editing, DPI setting, and reference links.
- Restored the GTK4 image properties dialog for rotate, flip, preview, proportional resize, and mouse-selected crop.
- Restored the GTK4 clipboard paste entry for text, including URL/file-path tagging in rich text and codebox YAML recognition.
- Adjusted GTK4 text clipboard export to prefer plain text over HTML when no CherryTree rich XML is available.
- Restored GTK4 clipboard multi-format providers/readers for internal rich text, codebox XML, table XML, `text/html`, and plain text.
- Restored GTK4 table row/column paste from the table clipboard target.
- Restored GTK4 `text/uri-list` paste into rich text using the existing link/image/embedded-file handling path.
- Connected the GTK4 text-view `DropTarget` signal and routed dropped URI/path strings through the same rich-text URI handling path.
- Restored GTK4 image clipboard paste from `Gdk::Clipboard::read_texture_async()`, converting `Gdk::Texture` to a pixbuf before inserting the image.
- Added GTK4 external file drag/drop ingestion through `Gtk::DropTargetAsync` and `Gdk::Drop::read_async()` for `text/uri-list`.
- Restored basic GTK4 treeview node drag/drop reordering using `Gtk::DragSource`, `Gtk::DropTarget`, and the existing `node_move_after` logic.
- Restored the GTK4 `Text and Code`, `Tree Explorer`, and `Toolbar` preference pages instead of showing temporary limited-page placeholders.
- Restored first-pass GTK4 popup menus through `Gio::Menu`/`Gtk::PopoverMenu` for tree nodes, rich text, code text, links, codeboxes, images, LaTeX, anchors, embedded files, and table cells.
- Restored GTK4 right-click/Menu-key entry points for the tree, main text view, codeboxes, heavy/light table cells, and anchored image/anchor/LaTeX/embedded-file widgets.
- Adjusted GTK4 right-click handling to use capture-phase menu controllers for the tree, main text view, codeboxes, and table cells so GTK's default text/entry menus do not appear beside CherryTree's menus.
- Switched GTK4 popup menu items to `win.*` actions and ensured every menu action is registered on the current window, so temporary right-click `Gtk::PopoverMenu` instances activate CherryTree actions reliably.
- Replaced GTK4 temporary right-click `Gtk::PopoverMenu` instances with direct `Gtk::Popover` button menus whose leaf rows call `CtMenuAction::run_action()` directly; this avoids action-group resolution problems in transient popup menus.
- Left ordinary GTK4 text input on GTK TextView's default IME path; custom key controllers now only handle CherryTree-specific shortcuts and delayed post-processing.
- On Windows GTK4, Vim mode is now opt-in for the current session at startup even if an older config saved `enable_vim_mode=true`, preserving normal IME entry as the default.
- Added `remember_vim_mode` and a GTK4 Text and Code preference checkbox so the user can explicitly choose whether the last Vim mode state should be restored on the next startup.
- Removed the manual `gtk_text_view_im_context_filter_keypress()` calls from custom GTK4 key controllers; normal text input is left to GTK TextView's default IME path.
- The Vim mode menu toggle now enables `remember_vim_mode` automatically, so a user who explicitly turns Vim mode on/off from the menu gets that state persisted across restart.
- Adjusted the GTK4 command palette layout by removing the temporary dialog buttons, expanding the command list area, and sizing the category/icon/label/shortcut columns more deliberately.
- Restored GTK4 tree keyboard navigation, row activation, expanded-state tracking, link hover tooltip updates, and context-menu coordinate handling with event controllers.
- Restored GTK4 text zoom from keyboard and Ctrl+scroll by reading and rewriting the configured rich/plain/code font descriptions instead of using the GTK3-only `StyleContext::get_font()`.
- Restored GTK4 main text-view click post-processing for link clicks, todo checkbox rotation, double-click automatic tag bounds, and triple-click paragraph selection.
- Restored GTK4 main text-view key handling for list Tab/Shift+Tab indentation, Ctrl+Space todo rotation/focus-to-anchor, and delayed post-key processing for smart quotes, auto links, symbol replacement, auto-indent, and list continuation.
- Restored GTK4 codebox inline toolbar buttons for execute, copy, and properties using a vertical `Gtk::Box`/`Gtk::Button` replacement for the GTK3 `Gtk::Toolbar`.
- Restored GTK4 `CtTextCell` post-processing controllers so codeboxes and heavy table text cells get link/todo click handling, double/triple-click handling, delayed key post-processing, and link hover tooltips.
- Restored GTK4 column editing by replacing GTK3-only `Gdk::Point` state with version-neutral point storage, removing the GTK4 no-op copy/cut/paste/edit stubs, and wiring Ctrl/Alt/mouse/focus controllers.
- Restored GTK4 storage-type selection, exit-save confirmation, code-execution confirmation, and tree summary dialogs instead of returning cancel/no-op from those flows.
- Restored GTK4 rich-text tag styling for bold, italic, underline, heading scalable styles, link underline, and paragraph justification using the scoped gtkmm4/Pango enum names.

Remaining before calling the migration complete:

- Manually run the GTK4 GUI and test create/open/save, rich text editing, tree selection, menu actions, clipboard, export basics, and ZeroMQ node focus.
- Manually verify GTK4 treeview drag/drop positions, including before/after/into drops and the "new parent can't be one of his children" guard.
- Manually verify the restored GTK4 preference pages, especially live application of text settings, tree icon updates, and toolbar add/remove/reorder.
- Manually verify GTK4 popup menus on tree nodes, text/code nodes, links, codeboxes, tables, images, anchors, LaTeX, and embedded files.
- Manually verify that GTK4 right-click shows only one popup menu on the tree, main editor, codeboxes, heavy/light tables, and anchored widgets.
- Manually verify Chinese IME composition/commit in the main editor, codeboxes, and table cells with Vim mode off, plus Vim Insert mode behavior when Vim mode is enabled manually.
- Manually verify Chinese IME entry again after removing the manual GTK4 IM filter, especially with Vim mode off and with Vim mode on in Insert mode.
- Manually verify GTK4 text editing details: Ctrl+mouse-wheel zoom, Ctrl+plus/minus/0 zoom, todo clicks, link clicks, Tab/Shift+Tab list levels, Enter list continuation, smart quotes, and symbol auto-replacement.
- Manually verify GTK4 column edit selection/editing, including Ctrl+Alt column selection, copy, cut, paste, insert, delete, and focus-loss cleanup.
- Manually verify GTK4 new/save storage selection, exit-save confirmation, code execution confirmation, tree summary dialog, and rich-text formatting restoration for bold/italic/underline/alignment.
- Manually verify GTK4 codebox inline toolbar buttons and heavy table text-cell post-processing.
- Verify Vim behavior interactively, including mode switching, movement, edit commands, command display, and `:w`.
- Manually verify the GTK4 command palette layout, search filtering, keyboard navigation, and command activation.
- Manually verify the new GTK4 Text and Code `Remember VIM Mode on Startup` option across restart with both checked and unchecked states.
- Decide whether GTK3 remains a supported build path or becomes a temporary compatibility path.

Known GTK4 implementation gaps still present:

- GTK4 context menus now have first-pass implementations, but terminal/VTE popup behavior remains blocked by the current GTK4 VTE integration placeholder.
- GTK4 context menus and popup behavior still need a full interactive GUI pass.
- Drag/drop and clipboard now have first-pass implementations, but image paste, file drops, internal rich-text moves, and tree drop positions still need manual GUI verification.
- Main text-view GTK4 click/key post-processing now has a first-pass implementation, but it still needs manual GUI verification against real text selections and Vim-mode interaction.
- Preferences no longer have obvious temporary limited-page placeholders, but they still need an interactive GTK4 pass before being considered production-ready.
- VTE cannot currently be verified on the local MSYS2 UCRT64 setup because `pkg-config` does not find `vte-2.91-gtk4` or `vte-2.91`; the Windows default build still has `USE_VTE=OFF`.

## Why GTK4

The built-in Vim path requires GtkSourceView 5.4 or newer. GtkSourceView 5 depends on GTK4, so Vim mode via `GtkSourceVimIMContext` means moving the editor path to GTK4/GtkSourceView5.

`GtkSourceVimIMContext` supports normal, insert, replace, visual, visual-line modes, command preview, search/replace, motions, text objects, registers, marks, and common Vim commands. That makes it a much better long-term base than hand-rolling Vim behavior on GTK3.

## Migration Strategy

Keep GTK3 building while bringing GTK4 up in parallel. Do not remove GTK3 code until GTK4 is functionally complete enough to replace it.

Use this order:

1. Make `WITH_GTK4=ON` compile.
2. Make the app start and open a simple document.
3. Restore editing workflows.
4. Restore CherryTree-specific rich text/object workflows.
5. Add built-in Vim mode.
6. Decide whether to keep dual GTK3/GTK4 support or remove GTK3.

## Phase 0 - Baseline And Branch Setup

Create a dedicated branch before touching migration code:

```powershell
git switch -c gtk4-migration
```

Capture the current GTK3 baseline:

```powershell
.\build.ps1 -Clean
.\build\cherrytree.exe --version
```

Record current local custom changes that must survive migration:

- ZeroMQ remote command receiver:
  - `src/ct/ct_zmq_remote.cc`
  - `src/ct/ct_zmq_remote.h`
  - `src/ct/ct_app.cc`
  - `src/ct/ct_app.h`
  - `CMakeLists.txt`
  - `src/ct/CMakeLists.txt`
- Windows build helper:
  - `build.ps1`
- no-latex packaging helper:
  - `package-nolatex.ps1`

Exit criteria:

- GTK3 build still passes.
- Git branch exists.
- A short baseline note exists in the first GTK4 migration commit message or a local scratch note.

## Phase 1 - Install GTK4 Build Dependencies

Install the GTK4 stack in MSYS2 UCRT64:

```powershell
$env:MSYSTEM='UCRT64'
$env:CHERE_INVOKING='1'
& C:\msys64\usr\bin\bash.exe -lc 'pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gtkmm-4.0 mingw-w64-ucrt-x86_64-gtksourceview5 mingw-w64-ucrt-x86_64-libspelling'
```

Verify:

```powershell
$env:MSYSTEM='UCRT64'
$env:CHERE_INVOKING='1'
& C:\msys64\usr\bin\bash.exe -lc 'pkg-config --modversion gtkmm-4.0 gtksourceview-5 libspelling-1'
```

Notes:

- `libspelling` replaces `gspell` in the GTK4 path.
- VTE is already off by default on Windows in this project, so terminal support is not a blocker.
- `libzmq` should remain unaffected by GTK4.

Exit criteria:

- `pkg-config` finds `gtkmm-4.0`.
- `pkg-config` finds `gtksourceview-5`.
- `pkg-config` finds `libspelling-1`, or CMake confirms spellcheck is disabled intentionally.

## Phase 2 - First GTK4 Compile

Run:

```powershell
.\build.ps1 -Gtk4 -Clean
```

Expected result:

- This may fail. The goal of this phase is to collect the compiler failure list and group failures by subsystem, not to fix everything in one sweep.

Failure triage groups:

- Build system and include paths.
- Removed GTK3 APIs.
- Event handling changes.
- Dialog API changes.
- Menu/action/shortcut changes.
- Tree/list model and view usage.
- Clipboard and drag-and-drop.
- Rich text anchored widgets.
- Theme/CSS/rendering.

Exit criteria:

- Build reaches C++ compilation.
- All first-wave errors are grouped into a checklist.
- No unrelated refactor is mixed into fixes.

## Phase 3 - Build System Cleanup

Tasks:

- Keep `WITH_GTK4` as a first-class option.
- Keep `BUILD_TESTING=OFF` as the default for quick Windows GTK4 migration builds.
- Add a clear dependency check message for GTK4 packages.
- Make `build.ps1 -Gtk4` print the chosen GTK stack.
- Keep `USE_ZMQ_REMOTE` independent from GTK version.

Potential CMake cleanup:

- Prefer target-scoped includes and definitions for new code, but avoid broad refactors unless needed.
- Keep `HAVE_LIBSPELLING` only when `libspelling-1` is found.
- Confirm `GTK_SOURCE_MAJOR_VERSION` and `GTK_SOURCE_MINOR_VERSION` are available where Vim mode is compiled.

Exit criteria:

- `.\build.ps1 -Gtk4` configures cleanly.
- `.\build.ps1` GTK3 path still configures cleanly.

## Phase 4 - Replace GTK3 Event Signals With GTK4 Controllers

GTK4 removed the old event signal style based on `GdkEvent*` for common widget input. This project has many legacy handlers behind `GTKMM_MAJOR_VERSION < 4`.

Main areas:

- Main text view:
  - `src/ct/ct_main_win_events.cc`
  - `src/ct/ct_text_view.cc`
  - `src/ct/ct_text_view.h`
- Codeboxes:
  - `src/ct/ct_codebox.cc`
  - `src/ct/ct_codebox.h`
- Tables:
  - `src/ct/ct_table.cc`
  - `src/ct/ct_table_light.cc`
  - `src/ct/ct_table.h`
- Tree view:
  - `src/ct/ct_main_win_events.cc`
  - `src/ct/ct_widgets.cc`
  - `src/ct/ct_widgets.h`

Implementation direction:

- Replace key handling with `Gtk::EventControllerKey`.
- Replace pointer press/release/motion with `Gtk::GestureClick` and `Gtk::EventControllerMotion`.
- Replace scroll handling with `Gtk::EventControllerScroll`.
- Keep legacy GTK3 handlers under `GTKMM_MAJOR_VERSION < 4` until GTK4 is stable.

Exit criteria:

- Main editor receives key events.
- Tree receives selection/navigation events.
- Mouse hover link tooltip still works.
- Ctrl+mouse zoom or equivalent zoom path works.
- Existing GTK3 event code is not broken.

## Phase 5 - Dialogs And Blocking Run Replacement

GTK4 moved away from the old `Gtk::Dialog::run()` pattern. The repo already has helper-style GTK4 blocking loops in several dialog files, but the migration needs to normalize this.

High-priority files:

- `src/ct/ct_dialogs.cc`
- `src/ct/ct_dialogs_gen_purp.cc`
- `src/ct/ct_dialogs_find.cc`
- `src/ct/ct_dialogs_link.cc`
- `src/ct/ct_dialogs_sel_node.cc`
- `src/ct/ct_dialogs_cmd_palette.cc`
- `src/ct/ct_dialogs_anch_widg.cc`
- `src/ct/ct_pref_dlg*.cc`

Tasks:

- Keep or create a shared helper equivalent to `run_dialog_blocking`.
- Replace dialog key-press handlers with controllers.
- Replace file chooser usage with GTK4-compatible `Gtk::FileChooserNative` or a local blocking wrapper.
- Restore disabled or minimal dialogs one at a time.

Known temporary GTK4 fallbacks or partial ports found now:

- No preference page currently shows the old temporary GTK4 limited-page placeholder.

Exit criteria:

- Start dialog works.
- Preferences dialog opens.
- Search/find dialogs open.
- Link dialog opens.
- Command palette opens and can execute a selected action.

## Phase 6 - Menu, Actions, Shortcuts, Toolbars

GTK4 favors `GAction`, `GMenuModel`, `Gtk::PopoverMenuBar`, `Gtk::MenuButton`, and application accelerators over GTK3 menus/accelerator groups.

Existing GTK4 work:

- `src/ct/ct_menu.cc` already has GTK4 toolbar/menu button builders.
- `src/ct/ct_main_win.cc` has `init_app_actions_gtk4()`.
- `src/ct/ct_main_win.h` has GTK4 menu fields.

Tasks:

- Audit all `CtMenuAction` registration.
- Ensure each action is available as a `Gio::SimpleAction` in GTK4.
- Ensure custom keyboard shortcuts still apply after preferences changes.
- Restore recent-docs menu.
- Restore bookmarks menu.
- Restore popup menus for text, tree nodes, tables, images, anchors, codeboxes.

Exit criteria:

- Main menu is visible and functional.
- Toolbar buttons trigger the same actions as GTK3.
- Keyboard shortcuts work globally.
- Context menus work for the main editing surfaces.

## Phase 7 - Tree And List Views

The current code still uses `Gtk::TreeView`, `Gtk::TreeStore`, and `Gtk::ListStore` widely. GTK4 still has transitional tree/list APIs in gtkmm, but long-term GTK4 code should move toward list models and factories only where needed.

Recommended approach:

- Do not rewrite the whole tree model first.
- Keep `CtTreeStore` and `CtTreeView` compiling on GTK4.
- Restore behavior before considering model modernization.

Current GTK4 status:

- Basic tree node drag/drop reordering is restored with GTK4 controllers and the existing tree move logic.
- Remaining verification should focus on drop-position accuracy, visual drop-row feedback, and edge autoscroll behavior.

Critical behavior:

- Open document and populate tree.
- Select node.
- Restore expanded/collapsed state.
- Drag/drop node reordering.
- Bookmarks.
- Tree tooltips.
- Header node buttons.

Exit criteria:

- Opening an existing `.ctb`/`.ctd` selects nodes reliably.
- Node switching does not lose text changes.
- Expanded/collapsed state is preserved.
- Node drag/drop reordering works for before, after, and child-drop positions.
- ZeroMQ node-id focusing still works in GTK4.

## Phase 8 - Clipboard, Drag-And-Drop, Rich Text Objects

The GTK4 path now has text paste restored in `ct_clipboard.cc`, including CherryTree rich XML recognition for internal rich-text copies. It also publishes and reads GTK4 `Gdk::ContentProvider` formats for internal rich text, codebox XML, table XML, `text/html`, `text/uri-list`, and plain text. Dropped URI/path strings are routed through the existing rich-text link/image/embedded-file handler. Image clipboard paste is restored through `Gdk::Clipboard::read_texture_async()`, and external file drag/drop now reads `text/uri-list` asynchronously through `Gtk::DropTargetAsync`/`Gdk::Drop::read_async()`.

Important files:

- `src/ct/ct_clipboard.cc`
- `src/ct/ct_clipboard.h`
- `src/ct/ct_text_view.cc`
- `src/ct/ct_treestore.cc`
- `src/ct/ct_image.cc`
- `src/ct/ct_table.cc`
- `src/ct/ct_table_light.cc`
- `src/ct/ct_codebox.cc`

Tasks:

- Restore plain text copy/cut/paste.
- Restore rich text copy/cut/paste.
- Restore image/object copy/paste.
- Restore URI/file drag-and-drop.
- Restore internal rich text drag-and-drop.
- Restore table cell paste and column paste.
- Verify anchored widgets render and relayout after node switch.

Exit criteria:

- Plain text and rich text roundtrip copy/paste work.
- Images and files can be inserted.
- Tables and codeboxes render after reload.
- Dragging files into the editor still inserts links/objects as expected.

## Phase 9 - Spellcheck

GTK3 uses `gspell`; GTK4 should use `libspelling`.

Existing hooks:

- `CMakeLists.txt` detects `libspelling-1`.
- `src/ct/ct_text_view.cc` has `_libspelling_rebuild_adapter`.
- `src/ct/ct_pref_dlg_rich_text.cc` has `HAVE_LIBSPELLING` checks.

Tasks:

- Verify `libspelling` runtime behavior on Windows.
- Ensure enabling/disabling spellcheck updates all open text views.
- Ensure language availability is detected.
- Verify right-click spelling menu in GTK4.

Exit criteria:

- Spellcheck can be toggled without crashing.
- No GTK3 `gspell` dependency is required in GTK4 builds.

## Phase 10 - Built-In Vim Mode

Once GTK4/GtkSourceView5 builds and the editor is usable, implement Vim mode via `GtkSourceVimIMContext`.

Current entry points:

- `src/ct/ct_actions_edit.cc`
  - `CtActions::toggle_ena_dis_vim_mode()` now toggles the main text view Vim mode and updates config state.
- `src/ct/ct_menu_actions.cc`
  - `vim_mode_toggle` is already gated behind GtkSourceView 5.4+.

Implemented first pass:

- Added `enableVimMode` / `enable_vim_mode` under `[editor]`.
- Store/load/update the flag in `CtConfig` and `CtMainWin`.
- Own a `GtkSourceVimIMContext` from `CtTextView`.
- Attach a GTK4 `Gtk::EventControllerKey` to the main node `GtkSourceView`.
- Use `gtk_event_controller_key_set_im_context`.
- Use `gtk_im_context_set_client_widget`.
- Bind `command-bar-text` and `command-text`.
- Display Vim status and command text in the status bar.
- Bind the Vim `write` signal to CherryTree's normal save action for plain `:w`.

Remaining implementation tasks:

- Verify whether the menu toggle needs to update multiple windows/text views in this fork's multi-window flow.
- Decide whether explicit `:w path` should stay unsupported or map to CherryTree Save As with format confirmation.

Scope decision:

- First pass: main node text view only.
- Second pass: codeboxes.
- Third pass: table cells if practical.

Exit criteria:

- Toggle appears in GTK4 builds.
- Toggle changes persistent config.
- `Esc`, `i`, normal movement, delete/yank/paste basics work through `GtkSourceVimIMContext`.
- Vim command text is visible somewhere unobtrusive, preferably the status bar.
- GTK3 builds hide or no-op this action cleanly.

## Phase 11 - Functional Verification Matrix

Run this matrix after each major phase.

Build checks:

```powershell
.\build.ps1 -Clean
.\build.ps1 -Gtk4 -Clean
```

Smoke checks:

- Start app with no file.
- Create a new document.
- Open an existing SQLite document.
- Open an existing XML document if available.
- Select nodes.
- Edit text.
- Save and reload.
- Copy/paste plain text.
- Copy/paste rich text.
- Insert image.
- Insert file/link.
- Insert table.
- Insert codebox.
- Search.
- Replace.
- Export HTML/TXT/PDF if relevant.
- Preferences dialog.
- Keyboard shortcuts dialog.
- Command palette.
- ZeroMQ node jump.
- Vim toggle.

Regression checks:

- GTK3 build still compiles until the final cutover decision.
- No data-loss behavior during node switch.
- No crash on document close.
- No crash on app exit while ZMQ receiver is running.

## Phase 12 - Decide Dual Support Or GTK4-Only

After GTK4 is usable, choose one path.

Option A: keep GTK3 and GTK4:

- Pros: safer fallback, easier comparison.
- Cons: more conditional code and more maintenance.

Option B: GTK4-only:

- Pros: cleaner code, easier Vim integration, less legacy API pressure.
- Cons: larger final patch, higher chance of regressions during cutover.

Recommended decision point:

- Keep dual support until Vim mode and the core document workflows are proven.
- Consider GTK4-only only after a complete manual verification pass.

## Risk Register

High risk:

- Rich text object anchoring and relayout.
- Clipboard and drag-and-drop.
- Tree node selection/state preservation.
- Dialog behavior changes caused by async GTK4 patterns.
- Context menus and keyboard shortcuts.

Medium risk:

- Spellcheck backend behavior on Windows.
- Theme/CSS differences.
- Printing/export previews.
- Table editing.
- Codebox editing.

Low risk:

- ZeroMQ receiver.
- Basic CMake dependency selection.
- Basic text buffer operations.

## Suggested Commit Sequence

1. `docs: add gtk4 migration plan`
2. `build: document and verify gtk4 dependency setup`
3. `build: make gtk4 configuration deterministic on windows`
4. `gtk4: fix first compile errors`
5. `gtk4: restore main window startup`
6. `gtk4: restore text view input controllers`
7. `gtk4: restore menus actions and shortcuts`
8. `gtk4: restore dialogs`
9. `gtk4: restore clipboard and drag drop`
10. `gtk4: restore rich text anchored widgets`
11. `gtk4: add gtksource vim im context wrapper`
12. `gtk4: persist and expose vim mode toggle`

## References

- GTK 3 to GTK 4 migration guide: https://docs.gtk.org/gtk4/migrating-3to4.html
- gtkmm 4 changes: https://gnome.pages.gitlab.gnome.org/gtkmm-documentation/changes-gtkmm4.html
- GtkSourceView 4 to 5 migration guide: https://gnome.pages.gitlab.gnome.org/gtksourceview/gtksourceview5/porting-guide-4-to-5.html
- GtkSourceVimIMContext: https://gnome.pages.gitlab.gnome.org/gtksourceview/gtksourceview5/class.VimIMContext.html
- MSYS2 gtksourceview5 package: https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-gtksourceview5
