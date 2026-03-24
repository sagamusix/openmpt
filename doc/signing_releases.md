
building signed updates
=======================

 *  A code signing certificate is assumed to be installed and available on the
    system.
 *  Run
    ```
    build\download_externals.cmd
    build\build_openmpt_release.cmd
    build\build_openmpt_release_retro.cmd
    ```

 *  results are found in `bin\openmpt-pkg.win-multi.tar` and
    `openmpt-pkg.win-retro.tar`.

 *  `openmpt/pkg.win/${BRANCHVERSION}/OpenMPT-${VERSION}-update.json` contains
    the update information that needs to be copied verbatim to the respective
    update channel on update.openmpt.org. This file is not signed as it itself
    is considered only informational and may be augmented with additional
    information. The files it links that contain actual code and automated
    update instructions are all signed.

 *  If the current user did not yet have a signing key on the local computer, a
    new key will be automatically generated and stored for future re-use in the
    encrypted Windows Key Store. The public key to verify the signatures is
    exported on each packaging of builds alongside the other build artefacts at
    `openmpt/pkg.win/${BRANCHVERSION}/OpenMPT-${VERSION}-update-publickey.jwk.json`
	. Any such new key should be added to the set of allowed update signing keys
    in the repository at `build/signingkeys/`, as an individual file named
    appropriately to describe the key (in order to easier identify the
    individual keys), and as a key in the jwkset of allowed keys in the file
    `build/signingkeys/signingkeys.jwkset.json`. A jwkset file consists of a
    JSON object containing a single array of all individual keys, named `"keys"`
    . The updated `signingkeys.jwkset.json` then needs to be copied to the https
    locations where the update check checks for the anchor keys. There is no
    separate key handling for test and release builds.
