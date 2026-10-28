Duplicate Finder (SSO-26642): Similar-group thumbnail tooltips now share the
same decode-guard policy (allocation limit, header-dimension cap) as the
similar-image hashing path, and no longer show a thumbnail for a file whose
header reports no size. Also fixes the Dimensions column and tooltip opening
the same file twice per row.
