# Neponset emulator context

There is no `emu_setup.json` here yet, on purpose: without a `neponset.xml` alongside it Scopy
would offer "neponset" in the emulator picker and then fail to start `iio-emu`.

The context XML has to be a real dump from the board. Hand-writing 28 devices would mean inventing
attribute names, enum spellings and current values, which is exactly what the band presets are
meant to be checked against - a fixture built from guesses would just confirm the guesses.

Dump it from a connected module (needs `python3-libiio`):

```bash
python -c "import iio; open('neponset.xml','w').write(iio.Context('ip:analog.local').xml)"
```

Drop the result in this directory, then add `emu_setup.json` next to it:

```json
[
    {
        "device": "neponset",
        "xml_path": "neponset.xml",
        "uri": "ip:127.0.0.1"
    }
]
```

`include_emu_xml` in the package `CMakeLists.txt` copies whatever is in this directory into the
built package, so no build changes are needed. Schema reference: `packages/ad936x/emu-xml/`.
