# INBOX — user ↔ agent messages

Append-only dialogue. The user appends messages here from anywhere (GitHub mobile editor, `git push` from a remote machine, terminal). The agent reads this file at the **very start** of every session — before picking the next experiment — and addresses any unacknowledged messages by appending a `> ack` line under them.

## Protocol

User messages format:
```
## YYYY-MM-DD HH:MM — <name>
<freeform text>
```

Agent acknowledgments format (appended immediately under the user message, never inline):
```
> ack YYYY-MM-DD HH:MM — <what the agent did about it: pivot, noted in NOTEBOOK, etc.>
```

Hard rules:
- Agent never deletes or rewrites user messages.
- Agent never edits its own past acks.
- If the user says STOP or similar, agent acks, does not start a new experiment, exits cleanly.

---

<!--
Example (commented out so it doesn't get processed):

## 2026-05-19 14:22 — philipp
Stop chasing point-to-plane variants for now. Try scan-context-style loop closure next.

> ack 2026-05-19 14:38 — pivoting; backlog reordered to put loop closure first.
-->
