# Blocker research and prototype decision — 2026-09-26

The user asked to fix blockers and finish the first ten days. Technical implementation proceeds through the master plan's explicitly isolated provisional fallback; this request is not treated as new survey evidence or historical approval.

## Source follow-up

- [Official Xiaobei Road primary-school archaeological survey, Yingyuan campus](https://wglj.gz.gov.cn/attachment/7/7969/7969705/10678436.pdf), 43 PDF pages, reviewed through the publisher's text: printed page 1 gives modern parcel corner coordinates; page 5 describes use of the city's coordinate and elevation framework. Those parcel corners are not persistent points identified on MAP-001. No datum-backed 1880–1900 walking surface was established from the material reviewed. No coordinate was inserted into the control table.
- [Official Guangzhou account of historic gates](https://www.gz.gov.cn/zlgz/whgz/content/post_8929405.html) is a context lead, not a surveyed control or height record. It does not resolve metric acceptance by itself.
- The additional official report URL `https://wglj.gz.gov.cn/attachment/7/7995/7995096/10747282.pdf` returned an internal retrieval error in this research pass; its contents were not used.

The existing `QA/Control_Survey_Handoff.md` remains the specific evidence request. The search was a targeted follow-up, not an exhaustive archive search or a claim that appropriate surveys do not exist.

## Engineering decision

Use `Data/Canton_Prototype_Contract.json` for the isolated modern-context map only. Choose the existing working origin, 2 m grid and EGM2008 0 m tool reference to make the prototype executable and reproducible. The original historical coordinate contract and historical tolerance requirements remain unchanged. Do not label unsurveyed points verified, lower the historical accuracy targets, or convert an unnamed datum to EGM2008.

The technical import must prove height encoding, row orientation, actor scale/origin, World Partition component configuration, edit-layer lock, persisted map load and collision. A passing technical prototype supports continued provisional work; it cannot establish historical M01 acceptance.
