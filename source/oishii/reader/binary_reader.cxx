#include "binary_reader.hxx"

namespace oishii {

//
// This is crappy, POC code -- plan on redoing it entirely
//

void BinaryReader::warnAt(const char* msg, u32 selectBegin, u32 selectEnd,
                          bool checkStack) {

  if (checkStack) // TODO, unintuitive limitation
  {
    // TODO: Warn class
    fprintf(stderr, "%s:0x%02X: ", getFile(), selectBegin);
    {
      ScopedFormatter fmt(0xe);
      fprintf(stderr, "warning: ");
    }

    fprintf(stderr, "%s\n", msg);
  } else
    fprintf(stderr, "\t\t");
  // Selection
  fprintf(stderr,
          "\tOffset\t00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n\t");

  if (!checkStack)
    fprintf(stderr, "\t\t");

  // We write it at 16 bit lines, a selection may go over multiple lines, so
  // this may not be the best approach
  u32 lineBegin = selectBegin / 16;
  u32 lineEnd = selectEnd / 16 + !!(selectEnd % 16);

  // Write hex lines
  for (u32 i = lineBegin; i < lineEnd; ++i) {
    fprintf(stderr, "%06X\t", i * 16);

    for (int j = 0; j < 16; ++j)
      fprintf(stderr, "%02X ",
              *reinterpret_cast<u8*>(getStreamStart() + i * 16 + j));

    for (int j = 0; j < 16; ++j) {
      const u8 c = *(getStreamStart() + lineBegin * 16 + j);
      fprintf(stderr, "%c", isprint(c) ? c : '.');
    }

    fprintf(stderr, "\n\t");
    fprintf(stderr, "      \t");
  }

  if (!checkStack)
    fprintf(stderr, "\t\t");

  for (u32 i = lineBegin * 16; i < selectBegin; ++i)
    fprintf(stderr, "   ");

  {
    ScopedFormatter fmt(0xa);

    fprintf(stderr, selectEnd - selectBegin == 0
                        ? "^ "
                        : "^~"); // one less, one over below
    for (u32 i = selectBegin + 1; i < selectEnd; ++i)
      fprintf(stderr, "~~~");
  }

  for (u32 i = selectEnd; i < lineEnd * 16; ++i)
    fprintf(stderr, "   ");

  fprintf(stderr, " ");

  for (u32 i = lineBegin * 16; i < selectBegin; ++i)
    fprintf(stderr, " ");

  {
    ScopedFormatter fmt(0xa);

    fprintf(stderr, "^");
    for (u32 i = selectBegin + 1; i < selectEnd; ++i)
      fprintf(stderr, "~");
  }
  fprintf(stderr, "\n");

  // Stack trace

  if (checkStack) {
    beginError();
    describeError("Warning", msg, "");
    addErrorStackTrace(selectBegin, selectEnd - selectBegin, "<root>");

    // printf("\tStack Trace\n\t===========\n");
    for (s32 i = mStack.mSize - 1; i >= 0; --i) {
      const auto& entry = mStack.mStack[i];
      printf("\t\tIn %s: start=0x%X, at=0x%X\n",
             entry.handlerName ? entry.handlerName : "?", entry.handlerStart,
             entry.jump);
      if (entry.jump != entry.handlerStart)
        addErrorStackTrace(entry.jump, entry.jump_sz, "indirection");
      addErrorStackTrace(entry.handlerStart, 0,
                         entry.handlerName ? entry.handlerName : "?");

      if (entry.jump != selectBegin &&
          (i == mStack.mSize - 1 || mStack.mStack[i + 1].jump != entry.jump))
        warnAt("STACK TRACE", entry.jump, entry.jump + entry.jump_sz, false);
    }

    endError();
  }
}

} // namespace oishii
