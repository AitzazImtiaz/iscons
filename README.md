## iscons
###### Integer Sequences [cons](https://oeis.org/wiki/Keywords)

The following tool attempts to solve the problem of maintenace in the OEIS after NIST updates its CODATA.
By the time I first ran this tool, it reported the following results:

```
SUMMARY: 123 consistent, 29 need update, 29 need fix, 148 missing, 34 skipped (fewer than 6 certain digits), 15152 cons sequences indexed.
```

The tool reported 29 constants published from NIST needs update to add more terms whilst 29 sequences need fixing from mistakes in sequences
while 123 constants were consistent to the latest CODATA. The following tool automates the verification process and is packed as a REPL.

 - **pulls** data from the OEIS sequence entries and the NIST constants
 - **checks** for the entries to match the OEIS sequences
 - **pushes** findings of malformed or incomplete sequences with missing constants in database

### Build
The following packages are required by debian-based systems:
`build-essential cmake pkg-config libreadline-dev git curl` For arch-based systems, these dependencies are required: `base-devel cmake pkgconf readline git curl`.

After cloning, provided the dependencies are met, use the following commands:

```
cd ~/iscons/build
cmake ..
make
sudo make install
```
