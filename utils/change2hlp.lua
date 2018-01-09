-- pandoc writer to convert a markdown'ed Penn change file into hlp format
--
-- Invoke with: pandoc -t change2hlp.lua
--
-- Note:  you need not have lua installed on your system to use this
-- custom writer.  However, if you do have lua installed, you can
-- use it to test changes to the script.  'lua sample.lua' will
-- produce informative error messages if your code contains
-- syntax errors.

local function escape(s, in_attribute)
   return s
end

-- Table to store footnotes, so they can be included at the end.
local notes = {}

-- Blocksep is used to separate block elements.
function Blocksep()
  return "\n\n"
end

-- This function is called once for the whole document. Parameters:
-- body is a string, metadata is a table, variables is a table.
-- This gives you a fragment.  You could use the metadata table to
-- fill variables in a custom lua template.  Or, pass `--template=...`
-- to pandoc, and pandoc will add do the template processing as
-- usual.
function Doc(body, metadata, variables)
   return body
end

-- The functions that follow render corresponding pandoc elements.
-- s is always a string, attr is always a table of attributes, and
-- items is always an array of strings (the items in a list).
-- Comments indicate the types of other variables.

function Str(s)
  return s
end

function Space()
  return " "
end

function SoftBreak()
  return "\n"
end

function LineBreak()
  return "\n"
end

function Emph(s)
  return "\x1B[1m" .. s .. "\x1B[0m"
end

function Strong(s)
   return "\x1B[1m" .. s .. "\x1B[0m"
end

function Subscript(s)
  return s
end

function Superscript(s)
  return s
end

function SmallCaps(s)
  return s
end

function Strikeout(s)
  return s
end

function Link(s, src, tit, attr)
   return s
end

function Image(s, src, tit, attr)
   return s
end

function Code(s, attr)
   return "\x1B[1m" .. s .. "\x1B[0m"
end

function InlineMath(s)
   return s
end

function DisplayMath(s)
   return s
end

function Note(s)
   return s
end

function Span(s, attr)
   return s
end

function RawInline(format, str)   
  if format == "html" then
    return str
  else
    return ''
  end
end

function Cite(s, cs)
   return s
end

function Plain(s)
  return s
end

function Para(s)
  return s .. "\n"
end

-- lev is an integer, the header level.
function Header(lev, s, attr)
   if lev == 1 then
      local vers, pl = string.match(s, "^Version ([^ ]+) patchlevel ([^ ]+)")
      return string.format("& %sp%s\n%s", vers, pl, s)
   else
      return s .. ":"
   end
end

function BlockQuote(s)
  return "\n" .. s .. "\n"
end

function HorizontalRule()
   return string.rep("-", 78)
end

function LineBlock(ls)
  return table.concat(ls, '\n')
end

function CodeBlock(s, attr)
   return s
end

function BulletList(items)
  local buffer = {}
  for _, item in pairs(items) do
    table.insert(buffer, "* " .. item)
  end
  return table.concat(buffer, "\n")
end

function OrderedList(items)
   local buffer = {}
   for num, item in pairs(items) do
      table.insert(buffer, string.format("%d. %s", num, item))
   end
   return table.concat(buffer, "\n")
end

function DefinitionList(items)
  local buffer = {}
  for _,item in pairs(items) do
    local k, v = next(item)
    table.insert(buffer, k .. ": " ..
                   table.concat(v, " "))
  end
  return table.concat(buffer, "\n")
end

function CaptionedImage(src, tit, caption, attr)
   return caption
end

-- Caption is a string, aligns is an array of strings,
-- widths is an array of floats, headers is an array of
-- strings, rows is an array of arrays of strings.
function Table(caption, aligns, widths, headers, rows)
   error("Tables not yet supported.", 0)
end

function RawBlock(format, str)
  if format == "html" then
    return str
  else
    return ''
  end
end

function Div(s, attr)
  return s
end

-- The following code will produce runtime warnings when you haven't defined
-- all of the functions you need for the custom writer, so it's useful
-- to include when you're working on a writer.
local meta = {}
meta.__index =
  function(_, key)
    io.stderr:write(string.format("WARNING: Undefined function '%s'\n",key))
    return function() return "" end
  end
setmetatable(_G, meta)

