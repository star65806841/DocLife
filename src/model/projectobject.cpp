#include "projectobject.h"

#include "projecmodel.h"

namespace doclife::model {
    ProjectObject::ProjectObject(QObject* parent)
        : QObject{ parent }
    {
    }

    ProjectObject::~ProjectObject()
    {
    }
    void ProjectObject::init()
    {
        m_projectData = std::make_unique<ProjectHead>();
    }
}