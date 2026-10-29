System Cleaner (SSO-26715): "Clean Selected" no longer prompts for the admin
password once per selected stale snap revision. Selecting multiple stale snap
revisions now removes them with a single elevated request, matching the
existing batched behavior for root-owned cache files.
