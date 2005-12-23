" Vim syntax file
" Language:	klone

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

if !exists("main_syntax")
  let main_syntax = 'klone'
endif

" Source HTML syntax
if version < 600
  source <sfile>:p:h/html.vim
else
  runtime! syntax/html.vim
endif
unlet b:current_syntax

" Next syntax items are case-sensitive
syn case match

" Include C syntax
syn include @cCode <sfile>:p:h/c.vim

syn region kloneDecl matchgroup=kloneTag start=/<%!/ keepend end=/%>/ contains=@cCode
syn region kloneScript matchgroup=kloneTag start=/<%/  keepend end=/%>/ contains=@cCode
syn region kloneExpr matchgroup=kloneTag start=/<%=/ keepend end=/%>/ contains=@cCode
syn region kloneDirective start=/<%@/ end=/%>/ contains=htmlString,kloneDirName,kloneDirArg

syn keyword kloneDirName contained include page taglib
syn keyword kloneDirArg contained file uri prefix language extends import session buffer autoFlush
syn keyword kloneDirArg contained isThreadSafe info errorPage contentType isErrorPage
syn keyword kloneCommandName contained include 
syn keyword kloneCommandArg contained name value 

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_klone_syn_inits")
  if version < 508
    let did_klone_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink htmlComment     Comment
  HiLink htmlCommentPart Comment
  " Be consistent with html highlight settings
  HiLink kloneTag          htmlTag
  HiLink kloneDirective    kloneTag
  HiLink kloneDirName      htmlTagName
  HiLink kloneDirArg       htmlArg
  HiLink kloneCommand      kloneTag
  HiLink kloneCommandName  htmlTagName
  "HiLink kloneCommandArg   htmlArg
  delcommand HiLink
endif

if main_syntax == 'klone'
  unlet main_syntax
endif

let b:current_syntax = "klone"

" vim: ts=8
