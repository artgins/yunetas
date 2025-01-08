# Api Reference

## Start up functions

## `add`

Add two numbers.

### Parameters
- `a` (_int or float_): The first number.
- `b` (_int or float_): The second number.

### Returns
- _int or float_: The sum of the numbers.

### Examples

```python
from my_module import add

result = add(5, 3)
```

## Function Documentation

### Description

This function performs a complex calculation involving matrix operations and returns the result.

### Parameters

- `matrix_a`: Input matrix A
- `matrix_b`: Input matrix B
- `result_matrix`: Output matrix storing the result

### Returns

The function returns the calculated result matrix.

### Implementation Details

The function uses optimized algorithms and SIMD instructions to perform fast matrix multiplication.

## Usage Example

**C Function:** `my_function(int a, int b)`

This function adds two integers together.

* **Args:**
    * `a`: The first integer.
    * `b`: The second integer.

* **Returns:**
    * The sum of `a` and `b`.

**Example:**

```c
#include <stdio.h>

int my_function(int a, int b) {
    return a + b;
}

int main() {
    int result = my_function(5, 3);
    printf("The sum is: %d\n", result);
    return 0;
}
```

| Attribute                    | Type          | Description                               |
|------------------------------|---------------|-------------------------------------------|
| `assignee_ids`               | integer array | IDs of the users to assign the issue to. Premium and Ultimate only. |
| `confidential`               | boolean       | Whether the issue is confidential or not. |
| `title`                      | string        | Title of the issue.                       |

| Attribute     | Type   | Required | Description |
|---------------|--------|----------|-------------|
| `widget_name` | string | No       | [Deprecated](https://link-to-issue) in GitLab 14.7 and is planned for removal in 15.4. Use `widget_id` instead. The name of the widget. |


> - `widget_name` [deprecated](https://link-to-issue) in GitLab 14.7.

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
