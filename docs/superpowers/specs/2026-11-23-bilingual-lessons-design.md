# Bilingual course material

## Intent and scope

Students cannot read the English-only lesson panel. Provide an immediately usable
Simplified Chinese / English course-language selector, defaulting to Chinese and
remembering the browser preference. Cover the 23 registered lessons, their three
progressive hints each, lesson navigation and course titles. Keep the existing
visual system, editing workflow, terminal, platform acceptance and Windows/WSL
architecture intact. This is course-content localization, not a whole-app English
UI conversion.

## Approach

Ship complete reviewed `*.zh-CN.md` siblings beside the English Markdown. Return
both translations in the existing read-only API payloads while preserving the
original English fields for existing clients. Derive Chinese course titles from
the translated lesson's first heading. Switch the already loaded text locally;
no new request or workspace action is needed when the learner changes language.

This implementation choice avoids online translation dependencies, preserves an
auditable English original and removes language-request races. Separate language
requests would save a few kilobytes but add asynchronous state handling without a
benefit for these small local files. Browser automatic translation cannot provide
consistent technical wording or reliable offline operation.

## Behavior

- Put a labelled native select beside the lesson title; use the existing colors.
- First visit uses `zh-CN`; a valid saved `en` preference survives reload. Storage
  denial or invalid preferences must not prevent reading or switching.
- Switch titles, body and the currently open hint immediately. Pending hint
  requests render using the latest preference. Preserve the selected course,
  active file, dirty buffers, running service, console, progress and history.
- Section shortcuts work with either language. File buttons continue to use the
  exact original file paths. Code blocks, identifiers, commands and protocol bytes
  are unchanged by translation.
- If a translation is missing, show the English original with a visible Chinese
  explanation, rather than labelling English text as Chinese. Unsafe, oversized
  or invalid UTF-8 translation files retain the existing server protections.
- At 1440px, 390px and 320px widths the selector remains reachable, labelled and
  at least 44px tall; the page must not gain horizontal overflow.

## Verification

Exercise the real server, state transitions and Chromium UI. Capture the same
calculator scene before and after, plus English and Chinese-hint states. Validate
coverage for all 23 lessons / 69 hints and exact fenced-code parity. Keep source
fingerprinting, formal tests and learner answers outside the localization change.
