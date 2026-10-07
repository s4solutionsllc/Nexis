Dashboard (SSO-25711, GH#493): the Health Score tile's "System Checkup" quick
action now actually opens the Maintenance Wizard. `HealthScoreTile::setQuickAction()`
was an empty stub, so the wizard had no working entry point from the dashboard.
