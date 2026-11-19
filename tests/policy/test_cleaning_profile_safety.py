#!/usr/bin/env python3
"""Safety-regression tests for the bundled cleaning profiles.

Runs without a Qt build: `python3 -m unittest discover -s tests/policy`, or
through CTest as `CleaningProfileSafety`.
"""

import re
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "scripts"))

import cleaning_profile_policy as policy_mod  # noqa: E402

REQUIRED_CLASSES = {"app-settings", "keys", "history", "databases", "toolchains", "offline-data"}


def profile(path, safety="safe", pid="synthetic"):
    return {"id": pid, "_file": "linux/synthetic.json", "safety": safety, "paths": [path]}


class BundledProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.profiles = policy_mod.load_profiles()
        cls.policy = policy_mod.load_policy()

    def test_bundled_profiles_exist(self):
        self.assertGreater(len(self.profiles), 30)

    def test_policy_still_covers_every_dangerous_class(self):
        self.assertEqual(REQUIRED_CLASSES, {c["id"] for c in self.policy["dangerousClasses"]})
        for cls in self.policy["dangerousClasses"]:
            self.assertTrue(cls["patterns"], cls["id"])

    def test_every_path_is_anchored_under_the_user_home(self):
        for p in self.profiles:
            for path in p["paths"]:
                self.assertTrue(policy_mod.normalize(path), f"{p['id']}: {path}")

    def test_no_profile_path_matches_the_deny_list(self):
        problems = [
            f"{p['id']}: {path} -> {denied} [{entry['id']}]"
            for p in self.profiles
            for path in p["paths"]
            for entry in self.policy["denyList"]
            for denied in policy_mod.denied_by(entry, path)
        ]
        self.assertEqual([], problems)

    def test_no_safe_profile_touches_a_dangerous_class(self):
        problems = [
            f"{p['id']}: {path} -> {cls['id']} '{pattern}'"
            for p in self.profiles
            if p["safety"] == "safe"
            for path in p["paths"]
            for cls in self.policy["dangerousClasses"]
            for pattern in policy_mod.class_hits(cls, path)
        ]
        self.assertEqual([], problems)

    def test_profile_ids_are_unique_per_platform(self):
        for platform in policy_mod.PLATFORMS:
            ids = [p["id"] for p in self.profiles if p["_file"].startswith(platform + "/")]
            self.assertEqual(len(ids), len(set(ids)), platform)

    def test_every_profile_file_is_in_the_resource_bundle(self):
        qrc = (REPO_ROOT / "shared" / "nexis" / "cleaning_profiles.qrc").read_text()
        bundled = set(re.findall(r"<file>cleaning_profiles/(.+?\.json)</file>", qrc))
        on_disk = {p["_file"] for p in self.profiles}
        self.assertEqual(on_disk, bundled)

    def test_generated_page_is_current(self):
        page = policy_mod.DOC_FILE.read_text(encoding="utf-8")
        self.assertEqual(
            policy_mod.render(self.profiles, self.policy),
            page,
            "docs/WHAT_NEXIS_TOUCHES.md is stale; run python3 scripts/cleaning_profile_policy.py",
        )


class PolicyMatcher(unittest.TestCase):
    """The checks above must fail when a profile is wrong, not pass vacuously."""

    @classmethod
    def setUpClass(cls):
        cls.policy = policy_mod.load_policy()

    def problems(self, path, safety="safe"):
        return policy_mod.violations([profile(path, safety)], self.policy)

    def test_deny_list_catches_credentials_at_any_depth_and_safety(self):
        for safety in ("safe", "aggressive"):
            self.assertTrue(self.problems("$HOME/.ssh", safety))
            self.assertTrue(self.problems("~/.ssh/*", safety))
            self.assertTrue(self.problems("$HOME/Library/Keychains/login.keychain-db", safety))

    def test_deny_list_normalises_placeholders(self):
        self.assertTrue(self.problems("$XDG_CONFIG_HOME/gcloud/credentials.db"))
        self.assertTrue(self.problems("${HOME}/.gnupg"))

    def test_deny_list_catches_an_ancestor_of_a_denied_location(self):
        self.assertTrue(self.problems("$HOME/.cargo", "aggressive"))
        self.assertTrue(self.problems("$HOME/Library/Developer/CoreSimulator", "aggressive"))

    def test_container_roots_cannot_be_deleted_or_wildcarded(self):
        for path in ("$HOME", "$HOME/.config", "$HOME/.config/*", "$HOME/Library/Caches/*",
                     "$XDG_CACHE_HOME/*"):
            self.assertTrue(self.problems(path, "aggressive"), path)

    def test_container_children_are_allowed(self):
        self.assertEqual([], self.problems("$HOME/Library/Caches/Homebrew/downloads"))
        self.assertEqual([], self.problems("$HOME/.config/*/Cache"))

    def test_unanchored_paths_are_rejected(self):
        self.assertTrue(self.problems("/etc/cache"))
        self.assertTrue(self.problems("$SOMETHING_ELSE/cache"))

    def test_safe_profile_cannot_touch_each_dangerous_class(self):
        samples = {
            "app-settings": "$HOME/.config/app/Local State",
            "keys": "$HOME/.config/app/Login Data",
            "history": "$HOME/.mozilla/firefox/*/places.sqlite",
            "databases": "$HOME/.config/app/IndexedDB",
            "toolchains": "$HOME/.rustup/toolchains",
            "offline-data": "$HOME/.m2/repository",
        }
        for cls, path in samples.items():
            self.assertTrue(self.problems(path), cls)

    def test_aggressive_profile_may_touch_offline_data(self):
        self.assertEqual([], self.problems("$HOME/.m2/repository", "aggressive"))
        self.assertEqual([], self.problems("$HOME/.nuget/packages", "aggressive"))

    def test_reclassified_locations_stay_out_of_safe(self):
        for path in ("$HOME/.nuget/packages", "$HOME/.gradle/caches/modules-2",
                     "$HOME/.yarn/berry/cache", "$HOME/.cache/spotify"):
            self.assertTrue(self.problems(path), path)

    def test_plain_caches_are_still_safe(self):
        for path in ("$HOME/.cache/pip", "$HOME/.npm/_cacache",
                     "$HOME/.config/Code/Cache", "$HOME/Library/Caches/Homebrew/downloads"):
            self.assertEqual([], self.problems(path), path)


if __name__ == "__main__":
    unittest.main()
