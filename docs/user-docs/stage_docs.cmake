# Stages copies of the repo's four user-facing markdown docs into
# docs/generated-user-docs-staging/, tagged so Doxygen's markdown-to-HTML
# conversion (docs/user-docs/Doxyfile) produces a predictable, stable
# output filename per page instead of its default absolute-path-derived
# hash (e.g. "md_docs_2..._2_u_s_e_r___g_u_i_d_e.html"). See
# docs/sound-mind-architecture.md's Decision #119 for why this exists -
# v0.0.42.4 (Workflow & Device Polish, Installment D).
#
# Two different tagging mechanisms, empirically confirmed against the
# installed Doxygen (1.18.0) - trying the more obvious one first:
# - README.md: an `<!-- \mainpage Title -->` HTML comment prepended as the
#   file's own first line - genuinely recognized this way (produces
#   index.html, the project's own front page - matching GitHub's own
#   convention of README as a repo's front page).
# - QUICKSTART.md/USER_GUIDE.md/CHANGELOG.md: a `\page`-in-HTML-comment
#   prepended the same way does NOT work for a plain `\page` (only
#   `\mainpage` gets that special first-comment recognition in a markdown
#   INPUT file) - confirmed by inspecting the actual generated output
#   (title stayed the raw filename, and the intended page name was never
#   registered anywhere - `\ref` to it failed everywhere, not just from
#   other files). Doxygen's own documented, working mechanism instead:
#   an explicit `{#id}` suffix on the file's own first real Markdown
#   heading - `# Quickstart` becomes `# Quickstart {#quickstart}` - which
#   Doxygen already promotes to that page's own name. Confirmed working
#   after switching to this for these three.
#
# Run via `cmake -P docs/user-docs/stage_docs.cmake` from the repo root (the
# `docs` CMake target's own first step for this pipeline) - paths below are
# relative to the current working directory, matching the existing
# docs/doxygen/Doxyfile's own repo-root-relative convention.

set(STAGING_DIR "docs/generated-user-docs-staging")
file(MAKE_DIRECTORY "${STAGING_DIR}")

function(rewrite_cross_links doc_content_var)
    set(doc_content "${${doc_content_var}}")

    # Cross-file markdown links (e.g. "[`QUICKSTART.md`](QUICKSTART.md)")
    # are correct as-is for GitHub's own rendering, but Doxygen's markdown
    # extension tries (and fails, since the target file's own page name
    # doesn't literally match "QUICKSTART.md"/"USER_GUIDE.md") to resolve
    # a `.md`-suffixed relative link as a cross-page reference on its own.
    # Rewritten here, in the staged copy only, as literal Doxygen `\ref`
    # commands (using the target page's own {#id} from below) - not
    # something that could appear in the real GitHub-rendered markdown, so
    # a plain, exact string match against each file's own one literal link
    # construct is safe.
    string(REPLACE "[`QUICKSTART.md`](QUICKSTART.md)" "\\ref quickstart \"QUICKSTART.md\""
           doc_content "${doc_content}")
    string(REPLACE "[`USER_GUIDE.md`](USER_GUIDE.md)" "\\ref user_guide \"USER_GUIDE.md\""
           doc_content "${doc_content}")

    # docs/sound-mind-design.md isn't one of the four docs this site
    # renders at all (it's the aspirational full design doc, not user-
    # facing documentation - see CLAUDE.md's own README/QUICKSTART/
    # USER_GUIDE distinction from it) - pointed at its real GitHub blob
    # instead, so the generated HTML's own link still actually works,
    # rather than a plain-text mention or a dead relative link.
    string(REPLACE "(docs/sound-mind-design.md)"
                   "(https://github.com/TheWalruss/sound-mind-2/blob/main/docs/sound-mind-design.md)"
                   doc_content "${doc_content}")

    set(${doc_content_var} "${doc_content}" PARENT_SCOPE)
endfunction()

# README.md - \mainpage via a prepended HTML comment (see this file's own
# docs above on why this mechanism, specifically, is the one that works).
file(READ "README.md" readme_content)
rewrite_cross_links(readme_content)
file(WRITE "${STAGING_DIR}/README.md" "<!-- \\mainpage Sound Mind Studio -->\n\n${readme_content}")

# QUICKSTART.md/USER_GUIDE.md/CHANGELOG.md - an explicit {#id} appended to
# each file's own first real heading (see this file's own docs above).
function(stage_doc_with_heading_id source_filename heading_line page_id)
    file(READ "${source_filename}" doc_content)
    rewrite_cross_links(doc_content)
    string(REPLACE "${heading_line}" "${heading_line} {#${page_id}}" doc_content "${doc_content}")
    file(WRITE "${STAGING_DIR}/${source_filename}" "${doc_content}")
endfunction()

stage_doc_with_heading_id("QUICKSTART.md" "# Quickstart" "quickstart")
stage_doc_with_heading_id("USER_GUIDE.md" "# Sound Mind Studio - User Guide" "user_guide")
stage_doc_with_heading_id("CHANGELOG.md" "# Changelog" "changelog")
