![version](https://img.shields.io/badge/version-18%2B-EB8E5F)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-diff-match-patch)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-diff-match-patch/total)

# 4d-plugin-diff-match-patch

Basic implementation of [google-diff-match-patch](https://github.com/google/diff-match-patch)

The Diff Match Patch plugin exposes Google's diff-match-patch algorithm to 4D as three commands: [`diff`](#diff), [`match`](#match), and [`patch`](#patch). Under the hood it drives a native, per-platform port of the same reference algorithm — a Qt/C++ port on Windows, an Objective-C port (`DiffMatchPatch.m`) on macOS — and every command returns a plain 4D `Text` value: an HTML-highlighted diff, a match index, or a serialized patch document, respectively.

> This reference describes the plugin's source as corrected and completed during a code-review/build session on 2026-08-01: a missing `default:` selector case was added, a `diffTimeout` edge case was fixed, and `match`/`patch` — previously unimplemented stubs — were written from scratch. If you're working against an already-compiled plugin binary, confirm it was actually built from this source before relying on the behavior below — in particular, `match` and `patch` may not exist at all in an older build.

## Summary

| Command | Returns | Purpose |
|---|---|---|
| [`diff`](#diff) | Text | Compute an HTML-highlighted difference between two texts |
| [`match`](#match) | Text | Fuzzy-search a pattern within a text, near an expected location |
| [`patch`](#patch) | Text | Generate a serialized patch document describing how to turn one text into another |

**Platforms:** Windows, macOS

---

## Requirements & platform notes

- Every command takes two mandatory `Text` parameters and one optional third `Object` parameter (`options`). Omitting the third parameter is always safe — every command falls back to the underlying library's built-in defaults, listed per-command below.
- **The `options` object's keys are shared across commands, but not all of them do anything for every command.** Each command below only reads and applies the subset of keys its own code path actually consumes; the rest are accepted without error but silently have no effect. Don't assume passing a key changes behavior just because it wasn't rejected — check each command's Description.
- Both ports implement the same reference algorithm, so output should be equivalent between Windows and macOS for well-formed input, but this hasn't been verified byte-for-byte across platforms.
- No selector currently leaves 4D waiting for a response that never arrives — every dispatch path, including an unrecognized one, returns a value. Earlier versions of this source did not guarantee that (see Error handling below).

---

## diff

### Syntax

```4d
diff ( text1 ; text2 { ; options } ) → Text
```

| Parameter | Type | Description |
|---|---|---|
| `text1` | Text | The original ("old") text. |
| `text2` | Text | The modified ("new") text. |
| `options` | Object | Optional. Tuning parameters — see table below. Omit to use the library's defaults. |
| Result | Text | An HTML fragment highlighting the differences between `text1` and `text2`. |

**`options` keys read by `diff`:**

| Key | Type | Default | Effect |
|---|---|---|---|
| `diffTimeout` | Real | `1.0` (seconds) | Maximum time to spend computing the diff before giving up and returning the best result so far. Only positive values are honored — see the note below. |

The other keys this command's `options` object will silently accept (`diffEditCost`, `patchMargin`, `patchDeleteThreshold`, `matchThreshold`, `matchDistance`, `matchMaxBits`) have **no effect on `diff`**: they configure the library's patch- and match-generation stages, which `diff` never invokes.

### Description

`diff` returns an HTML fragment: unchanged spans are wrapped in a plain `<span>`, insertions in `<span style="background-color:#e6ffe6;">`, and deletions in `<span style="background-color:#ffe6e6;">` (verified against the macOS port's `diff_prettyHtml`; presumed identical on Windows by convention, since this exact color scheme is the reference implementation's own, but not independently verified against that port's source). Newlines inside a diffed span are rendered as `&para;<br>` rather than a literal line break.

A `diffTimeout` of `0` or lower is **not** passed through to the underlying library as-is. The library itself treats `<= 0` as "no timeout" — i.e., it will let the diff run for an unbounded amount of time on two fully caller-controlled text blobs. This plugin now floors that: non-positive values are ignored and the library's own default (1.0 second) is used instead, so `diff` can no longer be made to run indefinitely via this option.

There's no parameter to cap input size — very large `text1`/`text2` values will simply take longer (bounded by `diffTimeout`) rather than being rejected.

### Example

From the plugin's own test method (`TEST.4dm`):

```4d
//%attributes = {}
$old:="ああああ"
$new:="あいいあ"

$diff:=diff($old; $new)
```

Rendering the result and tuning the timeout:

```4d
var $old; $new; $html : Text
var $options : Object

$old:="The quick brown fox"
$new:="The quick red fox jumps"

$options:=New object
$options.diffTimeout:=2  // give it up to 2 seconds instead of the 1s default

$html:=diff($old; $new; $options)
```

---

## match

### Syntax

```4d
match ( text ; pattern { ; options } ) → Text
```

| Parameter | Type | Description |
|---|---|---|
| `text` | Text | The text to search within. |
| `pattern` | Text | The (possibly imperfect) pattern to search for. |
| `options` | Object | Optional. Tuning parameters — see table below. Omit to use the library's defaults. |
| Result | Text | The 0-based character index of the best match, as text (e.g. `"12"`), or `"-1"` if no acceptable match was found. |

**`options` keys read by `match`:**

| Key | Type | Default | Effect |
|---|---|---|---|
| `near` | Real (integer) | `0` | Expected location of the match within `text`. The algorithm favors matches close to this position; negative values are clamped to `0`. **This key is not part of the underlying library's own vocabulary — it's this plugin's way of exposing the "search near this location" argument the library's `match_main` requires but the command's fixed 2-text-parameter signature has no room for.** |
| `matchThreshold` | Real | `0.5` | How loose a match is accepted: `0.0` requires a perfect match, `1.0` accepts almost anything. |
| `matchDistance` | Integer | `1000` | How far from `near` a match can be and still score well; a match this many characters away scores as if it added a full threshold point of error. |
| `matchMaxBits` | Integer | `32` | Maximum pattern length the search algorithm will accept. **On Windows**, a caller-supplied value is clamped to the range `1`–`64` before being applied (a defensive measure — this value sizes a bitmask internally, and an unvalidated out-of-range value was flagged as a potential undefined-behavior risk during code review). **On macOS**, this key has no effect at all: the underlying Objective-C port doesn't expose a public setter for it, so it's permanently fixed at its compiled-in default of `32`. |

`diffTimeout`, `diffEditCost`, `patchMargin`, and `patchDeleteThreshold` have **no effect on `match`** — they configure the diff and patch stages, which `match` never invokes.

### Description

If `pattern` is longer than the effective `matchMaxBits` (32 by default, or your clamped Windows override), `match` does not attempt the search at all — it returns `"-1"` rather than invoking the underlying library, since the library itself would reject an over-length pattern. Keep `pattern` short (well under `matchMaxBits` characters) for the fuzzy search to be meaningful; this algorithm is designed for locating a short, possibly-corrupted needle in a larger haystack, not for long-pattern matching.

There is no test file from the plugin's own project exercising `match` yet — the example below is illustrative, built only from the command's documented signature above.

### Example

```4d
var $text; $pattern; $result : Text
var $options : Object

$text:="The quick brown fox jumps over the lazy dog"
$pattern:="qick brwn fox"  // deliberately misspelled

$options:=New object
$options.near:=10
$options.matchThreshold:=0.6  // a bit looser than the 0.5 default

$result:=match($text; $pattern; $options)

If ($result="-1")
    ALERT("No sufficiently close match found.")
Else
    ALERT("Match found at position: "+$result)
End if
```

---

## patch

### Syntax

```4d
patch ( text1 ; text2 { ; options } ) → Text
```

| Parameter | Type | Description |
|---|---|---|
| `text1` | Text | The original text. |
| `text2` | Text | The modified text. |
| `options` | Object | Optional. Tuning parameters — see table below. Omit to use the library's defaults. |
| Result | Text | A serialized patch document describing how to turn `text1` into `text2`. |

**`options` keys read by `patch`:**

| Key | Type | Default | Effect |
|---|---|---|---|
| `diffTimeout` | Real | `1.0` (seconds) | `patch` computes a diff internally before building patches from it; this caps that internal diff step, with the same non-positive-value floor described under `diff` above. |
| `diffEditCost` | Integer | `4` | Cost (in edit characters) of an empty edit operation, used by the internal cleanup pass that simplifies the diff before it's turned into patches (only applied when the internal diff produces more than 2 diff segments). |
| `patchMargin` | Integer | `4` | Amount of unchanged context text kept around each change when the diff is chunked into individual patches. Larger values produce patches that apply more reliably against a slightly-shifted base text, at the cost of a larger patch document. |
| Result (`patchDeleteThreshold`, `matchThreshold`, `matchDistance`, `matchMaxBits`) | — | — | **Not read by `patch`.** These only matter when *applying* a previously-generated patch back onto a base text — a capability this plugin doesn't currently expose as a 4D command (there's no "apply" command in the manifest). Setting these keys in `patch`'s `options` object has no effect. |

### Description

The returned text is a GNU-diff-style unified patch: one `@@ -start1,len1 +start2,len2 @@` header line per hunk (1-based indices), followed by one line per diff segment prefixed with `+` (insertion), `-` (deletion), or a space (unchanged context), with the segment's text percent-escaped for URI-safety (verified against the macOS port's `Patch` serialization; presumed identical on Windows by the same reference-implementation convention as `diff`'s HTML output, not independently verified against that port's source).

`patch` only creates and serializes patches — it does not apply one to a base text. If you need to reconstruct `text2` from `text1` and a patch document, that requires an "apply" capability this plugin doesn't currently expose.

There is no test file from the plugin's own project exercising `patch` yet — the example below is illustrative, built only from the command's documented signature above.

### Example

```4d
var $old; $new; $patchText : Text
var $options : Object

$old:="Hello world."
$new:="Hello there, world!"

$options:=New object
$options.patchMargin:=8  // keep more context per hunk than the 4-character default

$patchText:=patch($old; $new; $options)

TRACE  // inspect $patchText in the debugger, or write it to a document
```

---

## Error handling & troubleshooting

- **Omitting the `options` parameter is always safe.** All three commands check whether it was passed before reading anything from it; there's no required key in any command's options object.
- **An unrecognized `options` key is never an error** — it's simply never read. A typo in a key name (e.g. `diffTimout`) will silently do nothing rather than raise an error in 4D, since the plugin has no way to detect it.
- **Setting a key that doesn't apply to the command you called is not an error either** — see each command's table above for exactly which keys it reads. `diff`, for instance, will accept a `patchMargin` key without complaint, but it has no effect.
- **`diffTimeout` of `0` or a negative number no longer disables the timeout.** Earlier versions of this plugin passed such values straight through, and the underlying library treats `<= 0` as "run with no time limit at all" — a caller could make `diff` or `patch` run indefinitely against large input. This is fixed in the current source: non-positive values are ignored and the library's 1-second default is used instead.
- **`match` returns `"-1"` both for "no match found" and for "pattern too long to search."** If you're getting `"-1"` unexpectedly, check `pattern`'s length against the effective `matchMaxBits` (32 by default) before assuming the search genuinely failed to find anything.
- **`matchMaxBits` cannot be changed on macOS**, regardless of what you pass — the underlying Objective-C port doesn't expose a way to change it, so it's always effectively `32` on that platform. It can be overridden on Windows (clamped to `1`–`64`).
- **An unrecognized plugin selector will not hang 4D.** This was a real risk earlier in this plugin's development (any selector without a matching `case` left 4D waiting for a return value that was never sent) but is fixed in the current source — every dispatch path now returns a value.

---

## Quick reference

```4d
// diff
$html:=diff($old; $new)
$html:=diff($old; $new; New object("diffTimeout"; 2))

// match
$loc:=match($text; $pattern)
$loc:=match($text; $pattern; New object("near"; 10; "matchThreshold"; 0.6))

// patch
$patchText:=patch($old; $new)
$patchText:=patch($old; $new; New object("patchMargin"; 8))
```

### Screenshot

<img width="421" alt="2018-02-14 15 49 21" src="https://user-images.githubusercontent.com/1725068/36197663-bed69022-11b7-11e8-8425-3dc7ae9fb62d.png">

instead of standard ``<ins>`` and ``<del>`` tags with ``background`` style, the library is modified to return ``<span>`` with ``background-color`` style, for better compatibility with 4D multi style text.
