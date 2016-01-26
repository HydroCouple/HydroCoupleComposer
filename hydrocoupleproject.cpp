#include "stdafx.h"
#include "hydrocoupleproject.h"
#include "gmodelcomponent.h"

HydroCoupleProject::HydroCoupleProject(const QFileInfo& file, QObject *parent)
	: QObject(parent)
{
	m_projectFile = file;

	if (file.exists())
	{
		readFile(file);
	}
}

HydroCoupleProject::~HydroCoupleProject()
{
	//qDeleteAll(m_connections);
	qDeleteAll(m_modelComponents);
	m_modelComponents.clear();
}

QFileInfo HydroCoupleProject::projectFile() const
{
	return m_projectFile;
}

void HydroCoupleProject::setProjectFile(const QFileInfo& file)
{
	m_projectFile = file;
}

QList<GModelComponent*> HydroCoupleProject::modelComponents() const
{
	return m_modelComponents;
}

//QList<GComponentConnection*> HydroCoupleProject::connections() const
//{
//	return m_connections;
//}

void HydroCoupleProject::addComponent(GModelComponent *& component)
{
	if (!contains(component))
	{
		m_modelComponents.append(component);
		connect(component, SIGNAL(setAsTrigger(GModelComponent*)), this, SLOT(onTriggerChanged(GModelComponent*)));
		emit componentAdded(component);
	}
}

bool HydroCoupleProject::deleteComponent(GModelComponent* & component)
{
	if (m_modelComponents.removeAll(component))
	{
		emit componentBeingDeleted(component);
		delete component;
		return true;
	}
	else
	{
		return false;
	}
}

void HydroCoupleProject::onTriggerChanged(GModelComponent* triggerModelComponent)
{
	for (QList<GModelComponent*>::Iterator it = m_modelComponents.begin();
		it != m_modelComponents.end(); it++)
	{
		if (*it != triggerModelComponent)
		{
			(*it)->setTrigger(false);
		}
	}
}

//bool HydroCoupleProject::removeComponent(GModelComponent * const component)
//{
//
//	for (int i = 0; i < m_components.count(); i++)
//	{
//		GModelComponent* t = m_components[i];
//
//		if (t->equals(component))
//		{
//			if (m_components.removeOne(t))
//			{
//				for (int j = 0; j < m_connections.count(); j++)
//				{
//					GComponentConnection* con = m_connections[j];
//
//					if (t->equals(con->producerComponent()) || t->equals(con->consumerComponent()))
//					{
//						removeConnection(con);
//
//						j--;
//					}
//				}
//
//				emit componentRemoved(t);
//
//				delete t;
//
//				return true;
//			}
//		}
//	}
//
//	return false;
//}
//
//void HydroCoupleProject::addConnection(GComponentConnection * const connection)
//{
//	for (int i = 0; i < m_connections.count(); i++)
//	{
//		if (connection->equals(m_connections[i]))
//		{
//			return;
//		}
//	}
//
//	m_connections.append(connection);
//
//	emit connectionAdded(connection);
//}

//bool HydroCoupleProject::removeConnection(GComponentConnection * const connection)
//{
//	for (int i = 0; i < m_connections.count(); i++)
//	{
//		GComponentConnection* conn = m_connections[i];
//
//		if (connection->equals(conn) && m_connections.removeOne(conn))
//		{
//			emit connectionRemoved(conn);
//
//			delete conn;
//
//			return true;
//		}
//	}
//
//	return false;
//}

bool HydroCoupleProject::contains(GModelComponent* component) const
{
	for (int i = 0; i < m_modelComponents.count(); i++)
	{
		GModelComponent* mcomponent = m_modelComponents[i];

		if (mcomponent == component)
		{
			return true;
			break;
		}
	}

	return false;
}

bool HydroCoupleProject::readFile(const QFileInfo& file)
{
	return true;
}
