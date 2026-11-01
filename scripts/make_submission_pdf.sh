#!/usr/bin/env bash
# Builds docs/submission/RA2311056010232_Assignment.pdf (must stay under 1 MB for the submission form).
# Needs: node + docx, pandoc, LibreOffice (soffice), qpdf. Run from the repo root.
set -euo pipefail
OUT=docs/submission; mkdir -p $OUT/tmp
SOFFICE="${SOFFICE:-soffice}"
node scripts/make_report.js
node scripts/make_submission_cover.js
# Markdown documents (the block-diagram image is already in the report, so drop it here).
grep -v '^!\[Block diagram\]' docs/embedded_design.md > $OUT/tmp/embedded_design.md
pandoc $OUT/tmp/embedded_design.md -o $OUT/tmp/embedded_design.docx
{ cat docs/ai_use_log.md; echo; echo '---'; echo; cat docs/ownership_statement.md; } > $OUT/tmp/ai_and_ownership.md
pandoc $OUT/tmp/ai_and_ownership.md -o $OUT/tmp/ai_and_ownership.docx
cp docs/report.docx $OUT/tmp/report.docx; cp $OUT/cover.docx $OUT/evidence.docx $OUT/tmp/
(cd $OUT/tmp && $SOFFICE --headless --convert-to pdf *.docx >/dev/null 2>&1)
qpdf --empty --pages $OUT/tmp/cover.pdf $OUT/tmp/report.pdf $OUT/tmp/embedded_design.pdf $OUT/tmp/evidence.pdf $OUT/tmp/ai_and_ownership.pdf -- \
     --object-streams=generate --compress-streams=y --recompress-flate --compression-level=9 $OUT/RA2311056010232_Assignment.pdf
rm -rf $OUT/tmp $OUT/cover.docx $OUT/evidence.docx
ls -l $OUT/RA2311056010232_Assignment.pdf
