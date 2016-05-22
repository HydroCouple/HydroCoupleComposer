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

