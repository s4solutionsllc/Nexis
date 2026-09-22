Startup Apps page, macOS BTM records section (GH#475 follow-up, SSO-25047):
each row's identifier/path subtitle (and name) had no wrap and no elision, so
a long value's sizeHint was captured as the QListWidgetItem's fixed size —
with the list's horizontal scrollbar off, that pushed the status badges (and
part of the row) past the viewport with no way to reach them. The row now
elides both labels to the list's actual width and shows the full text as a
tooltip; the list's horizontal scrollbar policy also switched to AsNeeded as
a defense-in-depth fallback.
