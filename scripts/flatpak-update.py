#!/usr/bin/env python3

# simple script to revise the flatpak manifest
# to allow git builds

import yaml

with open("org.openchemistry.Avogadro2.yaml") as stream:
    # use safe_load as it cannot execute arbitrary code
    document = yaml.safe_load(stream)
    for module in document["modules"]:
        if type(module) is not dict:
            continue

        if module["name"] == "avogadrolibs":
            for sources in module["sources"]:
                if type(sources) is not dict:
                    continue
                if sources["url"] == "https://github.com/OpenChemistry/avogadrolibs":
                    sources.pop("tag", None)
                    sources.pop("commit", None)
        elif module["name"] == "Avogadro2":
            for sources in module["sources"]:
                if type(sources) is not dict or "url" not in sources:
                    continue
                if sources["url"] == "https://github.com/OpenChemistry/avogadroapp":
                    sources.pop("tag", None)
                    sources.pop("commit", None)

with open("org.openchemistry.Avogadro2.yaml", "w") as stream:
    yaml.dump(document, stream)