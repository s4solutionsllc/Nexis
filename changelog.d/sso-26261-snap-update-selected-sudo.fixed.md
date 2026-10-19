Apt Source Manager (SSO-26261, GH#514): "Update Selected" now elevates via
pkexec when refreshing Snap packages, matching how every other privileged
package operation is run. Snap updates that previously failed with
"access denied" now succeed after authentication; declining the
authentication prompt surfaces a clear "Authentication cancelled" message
instead of the raw snapd stderr.
