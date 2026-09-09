# OTA feedback contract — 0.3.3

No board update starts automatically. The operator explicitly approves one board.
Network settings remain in LittleFS and are never included in OTA messages.

## Reporting

The server records REQUESTED before publishing the non-retained update command.
The board publishes retained `Control/<id>/ota/status` reports with `requestId`,
`version` (requested release commit), `runningVersion`, `feedbackVersion: 2`,
`state`, `detail`, and optional `progress` (0–100, transfer/write only).

Stages: ACCEPTED → DOWNLOADING → INSTALLING → REBOOTING → COMPLETE.
FAILED, REJECTED, CURRENT (no update needed), and UNCONFIRMED are separate outcomes.
100% transfer and an MQTT reconnect are not completion evidence.

Before accepting an update, write `/ota-attempt.json` atomically via a temporary
file. Abort if that fails; never format storage. After the updater successfully
finishes, save REBOOTING and its computed image MD5 before explicitly restarting.
On boot compare the running sketch MD5 to that record. Only a matching image
produces COMPLETE, including same-version reinstalls. MD5 here is an image
identity check, not a signature or protection against an untrusted server.
An interrupted attempt, corrupt record, or image mismatch is UNCONFIRMED.
The record is kept so the result can be reported again on MQTT reconnect and
periodically; a new accepted attempt replaces it. No flash writes per percentage.

The page keeps a visible sticky banner and progress bar, disables repeat update
actions during a fresh attempt, polls for up to five minutes, and reports missing
confirmation without assuming success. Closing/reopening resumes polling.
Server-side request IDs prevent a recent local request being overwritten by an
older retained attempt. A changed version alone is VERSION_CHANGED, not COMPLETE.

## Migration and staged physical test

0.3.2 cannot create this journal. The first installation of 0.3.3 from 0.3.2 may
still lack a verified completion report. Once 0.3.3 is running, explicitly approve
a same-version reinstall on the disconnected bench board to exercise the full flow.
Do not auto-retry or update installed layout boards.

Bench checks still required: successful same-version reinstall, MQTT reconnect
after successful boot, failed download, interrupted transfer (only with an agreed
recovery plan), unavailable storage, and image mismatch. Confirm private network
settings survive and that firmware reports COMPLETE only for a matching image.
