System Cleaner (GH#487 / SSO-25634): custom cleaning profiles dropped under
`~/.config/nexis/cleaning_profiles/` (Linux) or the platform's equivalent
app-config profiles directory now actually reach a scan. The
"Application Profiles" category had a complete backend (`CleaningProfilesService`,
the `APP_PROFILES` case in `CleanerService::scan()`) but was never wired into
the System Cleaner page's category cards or the schedule dialog's category
list, so it never ran. Both now include it.
