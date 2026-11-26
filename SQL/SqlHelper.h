#pragma once

#include <QString>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// SQL security and utility helpers
class SqlHelper
{
public:
    // Validate identifier (prevent SQL injection)
    static bool isValidIdentifier(const QString& name)
    {
        if (name.isEmpty()) return false;
        for (const QChar& c : name)
        {
            if (!c.isLetterOrNumber() && c != '_' && c != '-')
                return false;
        }
        return true;
    }

    // Safe SQL quoting
    static QString quoteIdentifier(const QString& name)
    {
        QString safe = name;
        safe.replace("\"", "\"\"");
        return "\"" + safe + "\"";
    }

    // Detect geometry column name from table
    static QString geometryColumnName(QSqlDatabase& db, const QString& tableName)
    {
        QSqlQuery query(db);
        query.exec(QString("PRAGMA table_info(%1)").arg(quoteIdentifier(tableName)));
        while (query.next())
        {
            QString col = query.value("name").toString().toLower();
            if (col == "geom" || col == "geometry")
                return query.value("name").toString();
        }
        // Try geometry_columns metadata
        query.prepare("SELECT f_geometry_column FROM geometry_columns WHERE f_table_name = ?");
        query.addBindValue(tableName);
        if (query.exec() && query.next())
            return query.value(0).toString();
        return "geom";
    }

    // Execute SQL with debug logging
    static bool execWithLog(QSqlQuery& query, const QString& sql)
    {
        bool ok = query.exec(sql);
        if (!ok)
            qDebug() << "[SQL] Error:" << query.lastError().text() << "| SQL:" << sql.left(200);
        return ok;
    }

    // Basic WHERE clause safety check
    static bool isSafeWhereClause(const QString& where)
    {
        QString lower = where.toLower().trimmed();
        // Block dangerous keywords
        QStringList dangerous = {"drop", "delete", "insert", "update", "alter", "create", "exec", "execute"};
        for (const QString& kw : dangerous)
        {
            if (lower.startsWith(kw + " ") || lower.contains(" " + kw + " ") || lower == kw)
                return false;
        }
        return true;
    }
};
