#!/usr/bin/env python3
"""Checks internal links.

    check_links.py BOOK_DIR       every relative href in the rendered book points to an
                                  existing page and, if it has one, an existing anchor
    check_links.py --markdown F.. every relative link in these Markdown files points to an
                                  existing file (anchors are checked for .md targets)

External (http, https, mailto) links are not fetched.
"""

import html.parser
import pathlib
import re
import sys
import urllib.parse


class PageParser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.hrefs = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            self.ids.add(attrs['id'])
        if tag == 'a' and attrs.get('href'):
            self.hrefs.append(attrs['href'])


def is_external(target):
    return re.match(r'^[a-z][a-z0-9+.-]*:', target) is not None


def check_book(root):
    pages = {}
    for path in root.rglob('*.html'):
        parser = PageParser()
        parser.feed(path.read_text(encoding='utf-8'))
        pages[path.resolve()] = parser
    errors = []
    for path, parser in pages.items():
        if path.name in ('print.html', '404.html', 'toc.html'):
            continue
        for href in parser.hrefs:
            if is_external(href):
                continue
            target, _, anchor = urllib.parse.unquote(href).partition('#')
            target_path = (path.parent / target).resolve() if target else path
            if target_path.is_dir():
                target_path = target_path / 'index.html'
            if target_path.suffix != '.html':
                if not target_path.exists():
                    errors.append(f'{path.relative_to(root.resolve())}: missing file {href}')
                continue
            page = pages.get(target_path)
            if page is None:
                errors.append(f'{path.relative_to(root.resolve())}: missing page {href}')
            elif anchor and anchor not in page.ids:
                errors.append(f'{path.relative_to(root.resolve())}: missing anchor {href}')
    return errors


def github_anchor(text):
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text).replace('`', '').strip().lower()
    return re.sub(r'[^\w\- ]', '', text).replace(' ', '-')


def markdown_anchors(path):
    anchors, seen, in_fence = set(), {}, False
    for line in path.read_text(encoding='utf-8').splitlines():
        if re.match(r'^\s*(```|~~~)', line):
            in_fence = not in_fence
            continue
        match = None if in_fence else re.match(r'^#{1,6}\s+(.*?)\s*$', line)
        if match:
            base = github_anchor(match.group(1))
            count = seen.get(base, 0)
            seen[base] = count + 1
            anchors.add(base if count == 0 else f'{base}-{count}')
    return anchors


def check_markdown(files):
    errors = []
    for path in files:
        in_fence = False
        for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            if re.match(r'^\s*(```|~~~)', line):
                in_fence = not in_fence
            if in_fence:
                continue
            line = re.sub(r'`[^`]*`', '', line)
            for target in re.findall(r'\]\(([^)\s]+)\)', line):
                if is_external(target):
                    continue
                file_part, _, anchor = target.partition('#')
                target_path = (path.parent / file_part) if file_part else path
                if not target_path.exists():
                    errors.append(f'{path}:{number}: missing file {target}')
                elif anchor and target_path.suffix == '.md' and anchor not in markdown_anchors(target_path):
                    errors.append(f'{path}:{number}: missing anchor {target}')
    return errors


def main(args):
    if args and args[0] == '--markdown':
        errors = check_markdown([pathlib.Path(a) for a in args[1:]])
    elif len(args) == 1:
        errors = check_book(pathlib.Path(args[0]))
    else:
        print(__doc__)
        return 2
    for error in errors:
        print(error)
    print(f'{len(errors)} broken link(s)')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
