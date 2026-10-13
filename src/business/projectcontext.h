#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include "model/projecmodel.h"

namespace doclife::core {

    /**
     * @brief 全局唯一的工程上下文（单例），持有多个 ProjectHead
     *
     * 模型：
     *  - 一个 ProjectHead  =  TreeView 上的一个顶层节点（一棵子树）
     *  - 多个 ProjectHead  =  TreeView 上的多个顶层节点
     *  - 整个 ProjectContext 就是这棵"多根树"的数据源
     *
     * 使用方式：
     *  - 读取全部：ProjectContext::instance().projects()
     *  - 监听变化：connect(&ctx, &ProjectContext::projectsChanged, ...)
     *  - 修改：addProject / updateProject / removeProject / clear
     */
    class ProjectContext : public QObject
    {
        Q_OBJECT

    public:
        // -------- 单例入口 --------
        static ProjectContext* instance();
        // 禁用拷贝与移动
        ProjectContext(const ProjectContext&) = delete;
        ProjectContext& operator=(const ProjectContext&) = delete;
        ProjectContext(ProjectContext&&) = delete;
        ProjectContext& operator=(ProjectContext&&) = delete;

        // ============================================================
        // 项目集合（只读访问）
        // ============================================================
        const QList<doclife::model::ProjectHead>& projects() const { return m_projects; }
        int  projectCount() const { return m_projects.size(); }
        bool isEmpty() const { return m_projects.isEmpty(); }

        QStringList projectIds() const;
        bool hasProject(const QString& id) const;

        // 按 id 取项目（不存在返回 nullptr）
        const doclife::model::ProjectHead* head(const QString& id) const;

        // 按索引取项目（越界返回 nullptr）
        const doclife::model::ProjectHead* headAt(int index) const;

        // 取得某项目在列表中的索引（不存在返回 -1）
        int indexOf(const QString& id) const;

        // ============================================================
        // 增 / 删 / 改
        // ============================================================
        // 新增项目；id 已存在则覆盖（不重复添加）
        void addNewProject(const QString& docName);
        void addProject(const doclife::model::ProjectHead& head);

        // 一次添加多个项目
        void addProjects(const QList<doclife::model::ProjectHead>& heads);

        // 按 head.id 更新已有项目；若 id 不存在则等同于 addProject
        void updateProject(const doclife::model::ProjectHead& head);

        // 删除项目（id 不存在则忽略）
        void removeProject(const QString& id);

        // 清空全部项目
        void clear();

        // ============================================================
        // 跨项目查询（在所有项目中查找）
        // ============================================================
        // 返回值包含：所属项目 id + 节点指针
        struct UnitRef { QString projectId; const doclife::model::ProjectUnit* unit = nullptr; };
        struct SubRef { QString projectId; const doclife::model::ProjectSub* sub = nullptr; };
        struct ItemRef { QString projectId; const doclife::model::ProjectItem* item = nullptr; };
        struct SheetRef { QString projectId; const doclife::model::Sheet* sheet = nullptr; };

        UnitRef  findUnit(const QString& id) const;
        SubRef   findSub(const QString& id) const;
        ItemRef  findItem(const QString& id) const;
        SheetRef findSheet(const QString& tableName, const QString& sheetName) const;

    signals:
        /**
         * 项目列表整体发生变化（增 / 删 / 清空 / 整体替换）。
         * TreeView 应重新构建模型。
         *
         * 注意：如果只是单个项目内容更新，不会发射此信号，
         * 而是发射 projectUpdated(id)，以便做局部刷新。
         */
        void projectsChanged();

        /**
         * 单个项目内容更新（updateProject 触发）。
         * 监听者可以只刷新对应子树。
         */
        void projectUpdated(const QString& projectId);

        /**
         * 项目被添加。
         */
        void projectAdded(const QString& projectId);

        /**
         * 项目被删除。
         */
        void projectRemoved(const QString& projectId);

    private:
        explicit ProjectContext(QObject* parent = nullptr);
        ~ProjectContext();

        doclife::model::ProjectHead* findMutable(const QString& id);

    private:
        QList<doclife::model::ProjectHead> m_projects;   // 保序，每个元素是树的一个顶点
    };

} // namespace doclife::core