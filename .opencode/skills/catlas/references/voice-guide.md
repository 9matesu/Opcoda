## Voice guide

Catlas docs read like notes from a senior engineer who just finished reading the code. Short sentences. Specific files. No filler.

### How to sound right

Write after you read the code. Cite `path:line` for each claim. Prefer verbs that describe what the code does: reads, writes, returns, drops, retries. Keep paragraphs short enough to scan.

Match the existing repo docs where they are good. In this workspace `AGENTS.md` and `.opencode/agent/test-engineer.md` carry the tone: direct, with the failure mode stated.

### Required pass

Load the `humanizer` skill before drafting. After drafting, run file mode over each markdown file. Keep code blocks, inline code, commands, paths, frontmatter, and link targets unchanged. Rewrite the prose around them.

Pay attention to the patterns that slip into technical docs:

* §1 contrasts of the form not X but Y. State the behavior directly.
* §2 closers that repeat the section. Cut them.
* §6 triads. Keep the number of items the code shows.
* §8 dashes used as connectors. Use commas, colons, or periods.
* §12 overused words such as delve, crucial, robust, showcase, landscape. Use plain words.
* §13 inflated significance. Keep the fact. Drop the claim that it marks a new era.
* §19 bold labels on every list item. Turn the list into prose or keep plain bullets.
* §20 title case headings and emojis. Use sentence case. No emojis in docs.

### Sentence case

Headings use sentence case: `## Data movement through the parser`. Straight quotes: `"PE"`. No chatbot wrappers: no great question, no hope this helps, no let me know.

### What stays technical

Keep exact error codes, exact paths, exact line numbers, exact commands. The humanizer pass must not soften a failure mode or round a number. A typed error such as `E_OOB` stays verbatim. A command such as `.\tools\build.ps1` stays verbatim.
