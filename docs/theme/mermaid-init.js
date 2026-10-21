// Renders <pre class="mermaid"> blocks produced by tools/mdbook_preprocessor.py.
(() => {
    const blocks = document.querySelectorAll('pre.mermaid');
    if (blocks.length === 0) {
        return;
    }
    const dark = ['coal', 'navy', 'ayu'].some((theme) => document.documentElement.classList.contains(theme));
    import('https://cdn.jsdelivr.net/npm/mermaid@10.9.1/dist/mermaid.esm.min.mjs').then(({ default: mermaid }) => {
        mermaid.initialize({ startOnLoad: false, theme: dark ? 'dark' : 'default' });
        mermaid.run({ nodes: blocks });
    });
})();
