# Learning workspace usability: reading, editing and results

## Approved scope

The user approved the three concrete improvements discussed against the actual
screenshots: reduce idle terminal space, reduce the controls before the lesson
and source, and make action feedback stay where the learner clicked. Keep the
existing visual style and Chinese/English course switching. Windows-native
migration, curriculum changes and acceptance rules are outside this task.

## Evidence and approach

Baseline `023e29b` reserves 460px for the desktop terminal even with no output.
Mobile topbar and remaining-height calculations disagree by 18px. Header,
toolbar, guidance and runner rows obscure source. The workflow action disappears
while the hidden toolbar button receives pending feedback.

Use the existing dock, panels and action handlers. A larger redesign or a new
tabbed workspace would change navigation beyond the approved scope; merely
reducing all font sizes would retain the crowded hierarchy. The implementation
choice is state-dependent dock sizing and compact existing component headers.

## Terminal behavior

- Initial / newly selected task: collapsed, visible title and status, <=72px.
- An actual output-producing operation opens normal size: desktop
  `clamp(200px, 30dvh, 320px)`, mobile `clamp(200px, 30dvh, 280px)`.
- A separate enlarge control offers about 52dvh for longer logs. Collapse and
  size controls remain keyboard accessible, with 44px mobile touch targets.
- Completion preserves a manual collapse during the current operation. Latest
  result and first useful error remain in the dock header. Next operation opens
  normally. Raw output and diagnostic links remain available, without mutation.
- Navigation, ordinary saves and silent service refresh do not open the dock.
- Console phase is independent of formal acceptance state. Runtime failures
  must never change a previous formal PASS.

## Learning space and action feedback

- Reserve remaining viewport height using the real topbar height and one dock
  size variable. Do not cover content with a terminal overlay.
- Keep one visible task title; hide only its duplicate first H1 in lesson prose.
  Keep section headings, bilingual navigation, hints and table scrolling.
- Put technical task metadata in an expandable `课程信息` detail. Keep prerequisites
  and stale/locked explanations accessible. Compact decorative headers and
  guidance spacing; do not reduce body reading text or touch target sizes.
- Ready workspace is a neutral status, not an active blue Start button. Failed
  workspace loading still offers retry. Do not steal editor focus on refresh.
- The clicked workflow action remains visible, disabled and `aria-busy` during
  save/build/run/check; then it becomes the actual next action. Preserve saved
  code checks, authoritative task refresh and existing failure recovery.

## Acceptance and evidence

Actual Chromium/engine fixtures compare untouched PR-base and changed pages at
the same task/state/viewport. Capture idle layout, output/failure and action
pending/result. Record geometry with screenshots: desktop1440x1000 has >=180px
visible code plus >=2 lines of prose; short1366x768 has >=120px code plus >=2
lines; mobile390/320 has >=2 lines of prose in the first learning viewport and
no page horizontal overflow. Verify default/normal/large/collapsed output,
summary, focus and pending recovery. Existing bilingual/browser and Node/Python
checks must still pass. Real screenshot review is required in addition to CI.
