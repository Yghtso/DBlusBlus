#!/usr/bin/env python3
"""mdBook preprocessor for the DBlusBlus documentation book.

It does two things when the book is rendered, without changing the Markdown sources:

1. Turns every architecture section reference such as `§4.13.3` into a link to that
   section's heading, in whichever chapter file it lives.
2. Turns ```mermaid code blocks into `<pre class="mermaid">` elements, which
   theme/mermaid-init.js renders as diagrams.

References inside code spans, code blocks and headings are left alone.
"""

import html
import json
import posixpath
import re
import sys

SECTION_HEADING = re.compile(r'^(#{1,6})\s+(\d+(?:\.\d+)*)\.?\s')
SECTION_REF = re.compile(r'§(\d+(?:\.\d+)*)')
FENCE = re.compile(r'^\s*(```|~~~)\s*(\S*)')
HEADING = re.compile(r'^#{1,6}\s+(.*?)\s*#*\s*$')


def heading_id(text):
    """Mirror mdBook's id generation for a heading's rendered text."""
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)  # links keep their text
    text = text.replace('`', '').replace('*', '')
    text = html.escape(text, quote=True)  # '&' renders as '&amp;', whose letters survive
    text = re.sub(r'<[^>]*>', '', text)
    out = []
    for ch in text:
        if ch.isalnum() or ch in '_-':
            out.append(ch.lower())
        elif ch.isspace():
            out.append('-')
    return ''.join(out)


def walk(items):
    for item in items:
        chapter = item.get('Chapter') if isinstance(item, dict) else None
        if chapter is None:
            continue
        yield chapter
        yield from walk(chapter.get('sub_items', []))


def lines_outside_code(content):
    """Yield (line, in_code_block, fence_info) for each line."""
    in_fence = False
    for line in content.split('\n'):
        match = FENCE.match(line)
        if match:
            yield line, True, match.group(2) if not in_fence else None
            in_fence = not in_fence
            continue
        yield line, in_fence, None


def section_targets(chapters):
    targets = {}
    for chapter in chapters:
        path = chapter.get('path')
        if not path or not path.startswith('architecture/'):
            continue
        seen = {}
        for line, in_code, _ in lines_outside_code(chapter['content']):
            if in_code:
                continue
            heading = HEADING.match(line)
            if not heading:
                continue
            base = heading_id(heading.group(1))
            count = seen.get(base, 0)
            seen[base] = count + 1
            anchor = base if count == 0 else f'{base}-{count}'
            numbered = SECTION_HEADING.match(line)
            if numbered:
                targets.setdefault(numbered.group(2), (path, anchor))
    return targets


def link_refs(text, here, targets):
    parts = re.split(r'(`+[^`]*`+)', text)  # keep code spans untouched
    for index in range(0, len(parts), 2):
        def replace(match):
            target = targets.get(match.group(1))
            if target is None:
                return match.group(0)
            path, anchor = target
            if path == here:
                return f'[{match.group(0)}](#{anchor})'
            relative = posixpath.relpath(path, posixpath.dirname(here) or '.')
            return f'[{match.group(0)}]({relative}#{anchor})'
        # Do not touch text that is already a link label.
        segments = re.split(r'(\[[^\]]*\]\([^)]*\))', parts[index])
        for seg in range(0, len(segments), 2):
            segments[seg] = SECTION_REF.sub(replace, segments[seg])
        parts[index] = ''.join(segments)
    return ''.join(parts)


def process(chapter, targets):
    out = []
    mermaid = None
    for line, in_code, info in lines_outside_code(chapter['content']):
        if mermaid is not None:
            if FENCE.match(line):
                out.append('<pre class="mermaid">' + html.escape('\n'.join(mermaid)) + '</pre>')
                mermaid = None
            else:
                mermaid.append(line)
            continue
        if info == 'mermaid':
            mermaid = []
            continue
        if in_code or HEADING.match(line):
            out.append(line)
        else:
            out.append(link_refs(line, chapter['path'], targets))
    chapter['content'] = '\n'.join(out)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == 'supports':
        sys.exit(0)
    _context, book = json.load(sys.stdin)
    items = book.get('sections', book.get('items', []))
    chapters = [c for c in walk(items) if c.get('path')]
    targets = section_targets(chapters)
    for chapter in chapters:
        process(chapter, targets)
    json.dump(book, sys.stdout)


if __name__ == '__main__':
    main()
