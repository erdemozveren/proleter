" Vim syntax file
" Language: Proleter

" Usage Instructions
" Put this file in .vim/syntax/proleter.vim
" Or ~/.config/nvim/syntax/proleter.vim
" and add in your .vimrc file the next line:
" autocmd BufRead,BufNewFile *.plt set filetype=proleter
" -------------
" For local use without copying this file to your config
" :set filetype=proleter
" :so /full/path/to/proleter.vim

if exists("b:current_syntax")
  finish
endif

syntax case match

" Strings
syntax region proleterString start=+"+ skip=+\\"+ end=+"+

" Keywords
syntax keyword proleterKeyword var func while if else return break continue
syntax keyword proleterBoolean true false
syntax keyword proleterConstant nil null

" Types
syntax keyword proleterType int string object bool float double

" Builtin forms
syntax match proleterImport "@import"

" Numbers
syntax match proleterNumber "\v<\d+>"
syntax match proleterNumber "\v<\d+\.\d+>"

" Function declaration name
syntax match proleterFunction "\vfunc\s+\zs[A-Za-z_][A-Za-z0-9_]*"

" Function calls
syntax match proleterFunctionCall "\v[A-Za-z_][A-Za-z0-9_]*\ze\s*\("

" Member calls/properties: math.randRange
syntax match proleterMember "\v\.[A-Za-z_][A-Za-z0-9_]*"

" Array type suffix: int[], int[3]
syntax match proleterArrayType "\v\[[0-9]*\]"

" Operators
syntax match proleterOperator "++"
syntax match proleterOperator "--"
syntax match proleterOperator "+="
syntax match proleterOperator "-="
syntax match proleterOperator "\*="
syntax match proleterOperator "/="
syntax match proleterOperator "%="
syntax match proleterOperator "=="
syntax match proleterOperator "!="
syntax match proleterOperator "<="
syntax match proleterOperator ">="
syntax match proleterOperator "||"
syntax match proleterOperator "&&"
syntax match proleterOperator "[-+*/%=<>!]"

" Delimiters
syntax match proleterDelimiter "[()[\]{},:;]"

" Comments - put near the end
syntax region proleterComment start="/\*" end="\*/" keepend contains=@Spell
syntax match proleterComment "//.*$" contains=@Spell

highlight def link proleterComment Comment
highlight def link proleterString String
highlight def link proleterKeyword Keyword
highlight def link proleterBoolean Boolean
highlight def link proleterConstant Constant
highlight def link proleterType Type
highlight def link proleterImport Include
highlight def link proleterNumber Number
highlight def link proleterFunction Function
highlight def link proleterFunctionCall Function
highlight def link proleterMember Function
highlight def link proleterArrayType Type
highlight def link proleterOperator Operator
highlight def link proleterDelimiter Delimiter

let b:current_syntax = "proleter"
