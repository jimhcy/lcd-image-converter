#include "modeparserbase.h"
//-----------------------------------------------------------------------------
namespace CommandLine {
//-----------------------------------------------------------------------------
ModeParserBase::ModeParserBase(QCommandLineParser *parser, QObject *parent) :
    QObject(parent)
{
    this->mParser = parser;
}
//-----------------------------------------------------------------------------
}
//-----------------------------------------------------------------------------
