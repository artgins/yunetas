# Api Reference

## Start up functions

# Sample Document

This is some introductory text.  
You can jump directly to the **Detailed Section** by clicking this link:  
[Go to Detailed Section](#detailed-section)

---

## Collapsible "Popup" Section

Sometimes you want to provide extra information without cluttering the page.  
You can achieve this using a `<details>` block:

<details>
  <summary>Click to see more details</summary>

Here is the detailed text that’s hidden by default.  
You can add **formatted text**, or even images and code blocks if you like!
</details>

---

## Detailed Section

This is the section you jumped to using the link above.  
You can put more content here.

# Sample Document

This is some introductory text.  
You can jump directly to the **Detailed Section** by clicking this link:  
[Go to Detailed Section](#detailed-section)

## Tooltip Example

To show a tooltip in the Sphinx Book Theme, you can use inline HTML like this:

Hover over this word:  
<span title="This is a tooltip that appears on hover!">hover here</span>.

Alternatively, you can use the `abbr` HTML element for abbreviations with tooltips:  
<abbr title="HyperText Markup Language">HTML</abbr>

## Collapsible Section

To provide expandable details, use Sphinx's `dropdown` directive:

```{dropdown} Click to see more details
Here is the detailed text that’s hidden by default.  
You can include **formatted text**, images, or code blocks:




---

### Key Points

1. **Tooltip Implementation**:  
   - Use `<span>` with a `title` attribute for basic tooltips.
   - Use `<abbr>` for semantic abbreviations with tooltips.

2. **Collapsible Details**:  
   - Sphinx Book Theme supports the `dropdown` directive for collapsible sections. This integrates smoothly into your Sphinx documentation.

3. **Linking to Sections**:  
   - Use Markdown syntax `[Link Text](#section-id)` to link to sections. Sphinx automatically assigns IDs to headings based on their text, converted to lowercase and with spaces replaced by hyphens.

### Rendering Notes

- Ensure your Sphinx configuration includes the Book Theme. Add it to `conf.py` like so:

```python
html_theme = "sphinx_book_theme"
