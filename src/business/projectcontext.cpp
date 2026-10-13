#include "ProjectContext.h"

#include <QApplication>

namespace doclife::core {

    // ---------------------------------------------------------------
    // 单例
    // ---------------------------------------------------------------
    ProjectContext* ProjectContext::instance()
    {
        // 用 qApp 作为 parent，QApplication 析构时自动删
        static ProjectContext* ctx = new ProjectContext(qApp);
        return ctx;
    }
    ProjectContext::~ProjectContext()
    {
        qDebug() << "[ProjectContext] destroyed";
    }
    ProjectContext::ProjectContext(QObject* parent)
        : QObject(parent)
    {
    }

    // ===============================================================
    // 只读访问
    // ===============================================================
    QStringList ProjectContext::projectIds() const
    {
        QStringList ids;
        ids.reserve(m_projects.size());
        for (const auto& p : m_projects) {
            ids.append(p.id);
        }
        return ids;
    }

    bool ProjectContext::hasProject(const QString& id) const
    {
        for (const auto& p : m_projects) {
            if (p.id == id) return true;
        }
        return false;
    }

    const doclife::model::ProjectHead* ProjectContext::head(const QString& id) const
    {
        for (const auto& p : m_projects) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }

    const doclife::model::ProjectHead* ProjectContext::headAt(int index) const
    {
        if (index < 0 || index >= m_projects.size()) return nullptr;
        return &m_projects[index];
    }

    int ProjectContext::indexOf(const QString& id) const
    {
        for (int i = 0; i < m_projects.size(); ++i) {
            if (m_projects[i].id == id) return i;
        }
        return -1;
    }

    void ProjectContext::addNewProject(const QString &docName)
    {
        doclife::model::Sheet sheet1;
        sheet1.id = "1234";
        sheet1.sheetName = "表1";
        doclife::model::Sheet sheet2;
        sheet2.id = "1235";
        sheet2.sheetName = "表2";

        doclife::model::ProjectItem item1;
        item1.id = "1234";
        item1.itemName = "分项工程1";

        item1.sheets.append(sheet1);
        item1.sheets.append(sheet2);

        doclife::model::ProjectSub sub1;
        sub1.id = "1234";
        sub1.subName = "分部工程1";
        sub1.items.append(item1);

        doclife::model::ProjectUnit unit1;
        unit1.id = "1234";
        unit1.unitName = "单位工程1";
        doclife::model::ProjectUnit unit2;
        unit2.id = "1235";
        unit2.unitName = "单位工程2";

        unit1.subs.append(sub1);
        unit2.subs.append(sub1);

        doclife::model::ProjectHead head;
        head.docName = docName;

        head.id = doclife::core::Utils::makeProjectId();

        head.units.append(unit1);
        head.units.append(unit2);
        m_projects.append(head);

        emit projectAdded(head.id);
        emit projectsChanged();
    }

    // ===============================================================
    // 增 / 删 / 改
    // ===============================================================
    void ProjectContext::addProject(const doclife::model::ProjectHead& head)
    {
        if (head.id.isEmpty()) return;   // 不允许空 id

        // 已存在 → 覆盖（不重复添加）
        if (auto* existing = findMutable(head.id)) {
            *existing = head;
            emit projectUpdated(head.id);
            return;
        }

        m_projects.append(head);
        emit projectAdded(head.id);
        emit projectsChanged();
    }

    void ProjectContext::addProjects(const QList<doclife::model::ProjectHead>& heads)
    {
        bool anyAdded = false;
        for (const auto& h : heads) {
            if (h.id.isEmpty()) continue;

            if (auto* existing = findMutable(h.id)) {
                *existing = h;
                emit projectUpdated(h.id);
            }
            else {
                m_projects.append(h);
                emit projectAdded(h.id);
                anyAdded = true;
            }
        }
        if (anyAdded) {
            emit projectsChanged();
        }
    }

    void ProjectContext::updateProject(const doclife::model::ProjectHead& head)
    {
        if (head.id.isEmpty()) return;

        if (auto* existing = findMutable(head.id)) {
            *existing = head;
            emit projectUpdated(head.id);
        }
        else {
            addProject(head);
        }
    }

    void ProjectContext::removeProject(const QString& id)
    {
        const int idx = indexOf(id);
        if (idx < 0) return;

        m_projects.removeAt(idx);
        emit projectRemoved(id);
        emit projectsChanged();
    }

    void ProjectContext::clear()
    {
        if (m_projects.isEmpty()) return;

        m_projects.clear();
        emit projectsChanged();
    }

    // ===============================================================
    // 内部
    // ===============================================================
    doclife::model::ProjectHead* ProjectContext::findMutable(const QString& id)
    {
        for (auto& p : m_projects) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }

    // ===============================================================
    // 跨项目查询
    // ===============================================================
    ProjectContext::UnitRef ProjectContext::findUnit(const QString& id) const
    {
        for (const auto& p : m_projects) {
            for (const auto& u : p.units) {
                if (u.id == id) return { p.id, &u };
            }
        }
        return {};
    }

    ProjectContext::SubRef ProjectContext::findSub(const QString& id) const
    {
        for (const auto& p : m_projects) {
            for (const auto& u : p.units) {
                for (const auto& s : u.subs) {
                    if (s.id == id) return { p.id, &s };
                }
            }
        }
        return {};
    }

    ProjectContext::ItemRef ProjectContext::findItem(const QString& id) const
    {
        for (const auto& p : m_projects) {
            for (const auto& u : p.units) {
                for (const auto& s : u.subs) {
                    for (const auto& i : s.items) {
                        if (i.id == id) return { p.id, &i };
                    }
                }
            }
        }
        return {};
    }

    ProjectContext::SheetRef ProjectContext::findSheet(const QString& tableName,
        const QString& sheetName) const
    {
        for (const auto& p : m_projects) {
            for (const auto& u : p.units) {
                for (const auto& s : u.subs) {
                    for (const auto& i : s.items) {
                        for (const auto& sh : i.sheets) {
                            if (sh.tableName == tableName && sh.sheetName == sheetName) {
                                return { p.id, &sh };
                            }
                        }
                    }
                }
            }
        }
        return {};
    }

} // namespace doclife::core