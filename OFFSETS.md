# Engage 2.0.0 (update v327680) — addresses

All addresses are main offsets (`g_main + off`, g_main = main's .text base). They come from `tools/il2dump.py` (`work/dump.txt`)
and match Cobalt's hardcoded offsets (e.g. `ProcInst.Jump` 0x281ec70).

## Hooks
| Address | What |
|---|---|
| 0x1f22b00 | `App.Pad.OnUpdate` — the game's pad update, once per frame on the game thread: our frame tick runs here |
| 0x245ed80 | `App.BasicMenu.TickInput` — runs for the menu that owns the input; records the focused menu |
| 0x20af610 | `nn.swkbd.Swkbd.ShowKeyboard(StringBuilder, ShowKeyboardArg, bool)` — debug build only: returns "Alear" |

## IL2CPP layout (metadata v27)
| Offset | What |
|---|---|
| obj+0x0 | klass |
| klass+0x10 / +0x18 | name / namespace (char*) |
| klass+0x58 | parent |
| klass+0xb8 | static fields |
| klass+0x138 + 16*slot | vtable: method pointer, +8 MethodInfo* |
| List<T> +0x10 / +0x18 | items array / size; array +0x18 length, +0x20 data |
| UnityEngine.Object +0x10 | m_CachedPtr (0 = destroyed native object: don't call into it) |

## App.BasicMenu (all 240 menus)
| Offset | What |
|---|---|
| +0x70 | m_menuContent (BasicMenuContent; +0x80 Cursor: +0x30 back cursor RectTransform, +0x38 back cursor Image) |
| +0x78 | m_menuItemList (List<BasicMenuItem>) |
| +0x98 / +0x9c | m_rowNum / m_showRowNum |
| +0xa0 | m_selectIndex |
| +0xa8 | m_scrollIndex |
| vslot 34 | SetSelectIndex(int) (what MoveDown calls) |
| vslot 59 | PlayCursorSE() |
| item +0x18 | m_menuItemContent (MonoBehaviour, its RectTransform is the item's box) |
| item +0x30 | m_attribute (bit 3 = hidden) |

UI canvases are Screen Space Overlay (render mode 0): world coordinates of a RectTransform are screen pixels (1920x1080 docked).

## Unity
| Address | What |
|---|---|
| 0x2c46440 | Component.get_transform |
| 0x2c46490 | Component.get_gameObject |
| 0x2c4eab0 | GameObject.get_activeInHierarchy |
| 0x2c4e880 | GameObject.get_transform |
| 0x2f7c3f0 | RectTransform.get_rect |
| 0x3792280 | Transform.TransformPoint(Vector3) |
| 0x37925a0 / 0x3792d40 / 0x37909f0 | Transform.childCount / GetChild / parent |
| 0x32e8e90 | Object.get_name |
| 0x2f8b920 / 0x2f8b960 | Screen.width / height |
| 0x318c240 | Graphic.get_canvas |
| 0x3f20c90 / 0x3f20370 / 0x3f20d90 | Canvas.rootCanvas / renderMode / worldCamera |
| 0x2c3c4b0 / 0x2c3c600 | Camera.WorldToScreenPoint / ScreenToWorldPoint (Vector3) |

## Game
| Address | What |
|---|---|
| 0x281aac0 | Proc.GetRoot(type 0..2): ProcInst trees (+0x20 name, +0x38 child, +0x48 next, +0x58 state, +0x5c suspend) |
| 0x299a510 | MapCamera.GetCamera() |
| 0x29bee00 / 0x29bbec0 / 0x29bc000 | MapCursor.GetPos / GetX / GetZ |
| 0x29bc380 | MapCursor.Set(int x, int z, float speed, bool updateEnterPos) |
| 0x29bec90 | MapCursor.TrySet(Vector3, float speed, bool) |
| 0x281eb10 | ProcInst.GetLabel (MapSequenceHuman labels: 1 FreeCursor, 4 PickCursorTick, 16 UnitCommand, 32 TargetSelect) |
| MapSequenceHuman +0xc0 | m_OperateMode: 1 Direct, 2 Indirect |
| 0x265b9f0 / 0x265baa0 | MapSequenceHuman.GetDefaultOperateMode (hooked) / SetOperateMode |
| 0x2662ab0 | MapSequenceHuman.UpdateRoute(x, z): needed after setting the cursor in PickCursorTick, or A moves nowhere |
| 0x1c4560 + slot 0x6284fb0 | SingletonClass<MapTarget>.get_Instance: slot -> metadata cell -> MethodInfo* |
| MapTarget +0x38 | m_DataSet: IndexOf(x, z) 0x1e422e0, get_Item(i) 0x1e42150 |
| MapSequenceTargetSelect +0x78 / +0x80 | m_TargetData / m_ItemIndex; TrySetTarget(data, item) 0x1f40310 |
| 0x1c53f80 | UnitPool.Get(i); Unit +0x50 m_Force (+0x20 type), +0xa0 / +0xa1 m_X / m_Z (sbyte) |
| 0x1bd9540 / 0x1bda5c0 | KeyHelpController / KeyHelpTitleBarController.SetKeyHelpMessage (hooked): +0x18 m_HelpObject, objects named after the button |
| 0x21ecaf0 | TitleBar.get_Instance; +0x7c m_IsShowHeader, +0x7e m_IsOpenHeader |
| Combat.CombatInput.Update 0x1ead6c0 | B or + (Pad statics +0x8 / +0x50) -> CombatSkip.Skip |
| Pad statics (klass+0xb8) | NpadButton values: A +0x0, B +0x8, X +0x10 ... Plus +0x50, Minus +0x58 |
| PlayerGenderSelectMenuSequence +0x78 | m_Gender: 0 none, Left -> 1, Right -> 2 (A needs a choice) |
| 0x3589420 | StringBuilder.Append(char) |
