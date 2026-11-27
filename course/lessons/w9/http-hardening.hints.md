# W9-A4 hints

## Hint 1 — Separate admission from grammar

Let the supplied parser handle message syntax. Count only field lines and cap the prefix before parsing. Test one complete and one partial request first.

## Hint 2 — Preserve the first message

A large receive buffer may hold many messages. Use a bounded prefix and return the parser's exact consumed count on acceptance. For a partial result with a complete header, inspect the declared length and require no partial request to escape.

## Hint 3 — Decide the status once

Implement a helper that constructs an empty rejected result with a required close. Classify header/body resource errors separately, check decoded body only when complete, and return a fresh incomplete result otherwise. Add boundary tests for exactly the limit and one byte beyond.
