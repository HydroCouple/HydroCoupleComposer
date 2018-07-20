/*!
 * \file   hydrocouple.h
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version   1.0.0
 * \description
 * This header file contains the core interface definitions for the
 * HydroCouple component-based modeling definitions.
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

#ifndef QXMLSYNTAXHIGHLIGHTER_H
#define QXMLSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextEdit>

class QXMLSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    QXMLSyntaxHighlighter(QObject * parent);
    QXMLSyntaxHighlighter(QTextDocument * parent);
    QXMLSyntaxHighlighter(QTextEdit * parent);

    ~QXMLSyntaxHighlighter();

protected:
    void highlightBlock(const QString & text) override;

    QTextCharFormat* keywordFormat() const;

    QTextCharFormat* elementFormat() const;

    QTextCharFormat* attributeFormat() const;

    QTextCharFormat* valueFormat() const;

    QTextCharFormat* commentFormat() const;

private:
    void highlightByRegex(const QTextCharFormat & format,
                          const QRegExp & regex, const QString & text);

    void initializeRegexes();

    void initializeFormats();

private:
    QTextCharFormat *m_xmlKeywordFormat;
    QTextCharFormat *m_xmlElementFormat;
    QTextCharFormat *m_xmlAttributeFormat;
    QTextCharFormat *m_xmlValueFormat;
    QTextCharFormat *m_xmlCommentFormat;

    QList<QRegExp>      m_xmlKeywordRegexes;
    QRegExp             m_xmlElementRegex;
    QRegExp             m_xmlAttributeRegex;
    QRegExp             m_xmlValueRegex;
    QRegExp             m_xmlCommentRegex;
};

#endif // QXMLSYNTAXHIGHLIGHTER_H

