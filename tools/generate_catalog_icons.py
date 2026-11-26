"""Generate the original, compact SVG icon set used by both catalog trees."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / "Resources" / "catalog_icons"


def svg(body: str) -> str:
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" '
        'viewBox="0 0 24 24">\n'
        '<g stroke-linecap="round" stroke-linejoin="round">\n'
        f'{body}\n</g>\n</svg>\n'
    )


def folder(color: str, emblem: str = "") -> str:
    return svg(
        f'<path d="M2.5 7.5V5.8c0-.8.7-1.5 1.5-1.5h5l2 2h8.8c.9 0 1.7.8 1.7 1.7v1" '
        f'fill="none" stroke="{color}" stroke-width="1.6"/>'
        f'<path d="M3.4 8.5h17.2c.8 0 1.3.7 1.1 1.5l-2.2 9c-.2.7-.8 1.2-1.5 1.2H4.3c-.7 0-1.2-.5-1.4-1.2L2.3 10c-.1-.8.4-1.5 1.1-1.5Z" '
        f'fill="{color}" fill-opacity=".13" stroke="{color}" stroke-width="1.6"/>'
        + emblem
    )


def paper(color: str, emblem: str = "") -> str:
    return svg(
        f'<path d="M5 2.8h9.2l4.8 4.8V20c0 .7-.6 1.2-1.2 1.2H5c-.7 0-1.2-.5-1.2-1.2V4c0-.7.5-1.2 1.2-1.2Z" '
        f'fill="{color}" fill-opacity=".11" stroke="{color}" stroke-width="1.6"/>'
        f'<path d="M14.2 2.8v4.8H19" fill="none" stroke="{color}" stroke-width="1.6"/>'
        + emblem
    )


ICONS = {
    "database": svg('<ellipse cx="12" cy="5.3" rx="8.3" ry="2.7" fill="#D8F2EE" stroke="#168476" stroke-width="1.6"/><path d="M3.7 5.3v12.8c0 1.5 3.7 2.8 8.3 2.8s8.3-1.3 8.3-2.8V5.3M3.7 11.7c0 1.5 3.7 2.8 8.3 2.8s8.3-1.3 8.3-2.8" fill="none" stroke="#168476" stroke-width="1.6"/><path d="M3.7 17.1c0 1.5 3.7 2.8 8.3 2.8s8.3-1.3 8.3-2.8" fill="none" stroke="#168476" stroke-width="1.6"/>'),
    "folder": folder("#6A8092", '<path d="M7 14h10" stroke="#6A8092" stroke-width="1.6"/>'),
    "results": folder("#356FAD", '<path d="m7 16 3-3 2 2 2.4-3 2.6 4H7Z" fill="#356FAD"/><circle cx="9" cy="11.3" r="1" fill="#F6BD4B"/>'),
    "elements": folder("#168476", '<circle cx="8" cy="13" r="1.4" fill="#168476"/><circle cx="16" cy="12" r="1.4" fill="#168476"/><circle cx="13" cy="17" r="1.4" fill="#168476"/><path d="m9.4 13 5.2-.8m.5 1.1-1.3 2.3m-1.7.6-3.2-2" fill="none" stroke="#168476" stroke-width="1.2"/>'),
    "materials": folder("#7956A8", '<path d="M7.5 12h8m-8 2.5h8m-8 2.5h5.2" stroke="#7956A8" stroke-width="1.4"/>'),
    "ai": svg('<rect x="2.8" y="2.8" width="18.4" height="18.4" rx="3.1" fill="#352032" stroke="#EE7627" stroke-width="1.4"/><text x="5.4" y="16.2" font-family="Arial,sans-serif" font-size="11.5" font-weight="700" fill="#FF9B47">Ai</text>'),
    "pdf": paper("#C74045", '<rect x="4.2" y="12.2" width="16.8" height="6.1" rx="1" fill="#C74045"/><text x="5.4" y="16.8" font-family="Arial,sans-serif" font-size="5.3" font-weight="700" fill="#FFFFFF">PDF</text>'),
    "other": folder("#73808A", '<circle cx="9" cy="14.1" r="1" fill="#73808A"/><circle cx="12" cy="14.1" r="1" fill="#73808A"/><circle cx="15" cy="14.1" r="1" fill="#73808A"/>'),
    "imagery": folder("#278B9E", '<path d="m6.5 17 3.1-3.2 2.1 1.8 2.7-3.3 3.1 4.7H6.5Z" fill="#278B9E"/><circle cx="8.4" cy="11.5" r="1.1" fill="#E4A54B"/>'),
    "hillshade": folder("#7B6F8C", '<path d="m6.4 17 4-5.3 2.4 3 2.4-2.7 2.5 5H6.4Z" fill="#7B6F8C"/><path d="M9.4 16.9c1.2-1 2-1.8 2.8-3.2" fill="none" stroke="#FFFFFF" stroke-width=".9"/>'),
    "feature_set": folder("#188A71", '<rect x="7.1" y="11.2" width="4.1" height="4.1" rx=".5" fill="#188A71"/><rect x="13" y="13.1" width="4.1" height="4.1" rx=".5" fill="none" stroke="#188A71" stroke-width="1.3"/>'),
    "document": paper("#4B78AF", '<path d="M7 10.4h9M7 13.5h9M7 16.6h6.2" stroke="#4B78AF" stroke-width="1.4"/>'),
    "spreadsheet": paper("#168B66", '<path d="M7 10.2h9.4v7.4H7zM10.1 10.2v7.4m3.2-7.4v7.4M7 13.8h9.4" fill="none" stroke="#168B66" stroke-width="1.1"/>'),
    "file": paper("#718395", '<path d="M7 11h9M7 14h9M7 17h5.5" stroke="#718395" stroke-width="1.3"/>'),
    "point": svg('<circle cx="12" cy="12" r="7.5" fill="#E6F3FC" stroke="#326DA7" stroke-width="1.5"/><circle cx="12" cy="12" r="3.2" fill="#E4A648" stroke="#326DA7" stroke-width="1.2"/>'),
    "line": svg('<path d="m3.5 18.2 5.1-8 4.5 4 6.9-8.2" fill="none" stroke="#3575B2" stroke-width="2"/><circle cx="3.5" cy="18.2" r="1.7" fill="#FFFFFF" stroke="#3575B2" stroke-width="1.3"/><circle cx="8.6" cy="10.2" r="1.7" fill="#FFFFFF" stroke="#3575B2" stroke-width="1.3"/><circle cx="13.1" cy="14.2" r="1.7" fill="#FFFFFF" stroke="#3575B2" stroke-width="1.3"/><circle cx="20" cy="6" r="1.7" fill="#FFFFFF" stroke="#3575B2" stroke-width="1.3"/>'),
    "polygon": svg('<path d="m4.2 17.2 2.1-10.4 9.3-2.2 4.2 9.3-7.3 5.3Z" fill="#D7EBDD" stroke="#278A71" stroke-width="1.6"/><circle cx="4.2" cy="17.2" r="1.2" fill="#278A71"/><circle cx="6.3" cy="6.8" r="1.2" fill="#278A71"/><circle cx="15.6" cy="4.6" r="1.2" fill="#278A71"/><circle cx="19.8" cy="13.9" r="1.2" fill="#278A71"/>'),
    "raster": svg('<rect x="3.5" y="3.5" width="17" height="17" rx="1.5" fill="#EAF0F6" stroke="#567791" stroke-width="1.5"/><path d="M9.2 3.5v17M14.8 3.5v17M3.5 9.2h17M3.5 14.8h17" stroke="#8EA5B9" stroke-width=".8"/><path d="m4.4 18.6 5.1-5 3.1 2.6 3.4-5.2 3.5 4.7v2.9Z" fill="#6290B0"/>'),
    "table": svg('<rect x="3.5" y="4" width="17" height="16" rx="1.4" fill="#F0F6F4" stroke="#3A8B76" stroke-width="1.5"/><path d="M3.5 9h17M3.5 14.4h17M9.1 9v11M15 9v11" stroke="#3A8B76" stroke-width="1.1"/>'),
}


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    for name, content in ICONS.items():
        (ROOT / f"{name}.svg").write_text(content, encoding="utf-8")
    print(f"Generated {len(ICONS)} catalog icons in {ROOT}")
