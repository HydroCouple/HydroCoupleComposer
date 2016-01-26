#ifndef HYDROCOUPLEPROJECT_H
#define HYDROCOUPLEPROJECT_H

#include <QObject>


class GModelComponent;

class HydroCoupleProject : public QObject
{
	Q_OBJECT;
	Q_PROPERTY(QFileInfo ProjectFile READ projectFile WRITE setProjectFile);
	Q_PROPERTY(QList<GModelComponent*> ModelComponents READ modelComponents);
	//Q_PROPERTY(QList<GComponentConnection*> Connections READ connections);

public:
	HydroCoupleProject(const QFileInfo& file, QObject *parent);

	virtual ~HydroCoupleProject();

	//!File to save project in
	QFileInfo projectFile() const;

	//!Set file to save project in
	void setProjectFile(const QFileInfo& file);

	//!Instantiated model components
	QList<GModelComponent*> modelComponents() const;

	//!Instantiated model connections
	//QList<GComponentConnection*> connections() const;

	//!add instantiated component to list of components
	void addComponent(GModelComponent *& component);

	//!remove component from list of instantiated components
	bool deleteComponent(GModelComponent *& component);

	//!add instantiated component to list of components
	//void addConnection(GComponentConnection * const connection);

	//!remove component from list of instantiated components
	//bool removeConnection(GComponentConnection * const connection);

signals:
	//! emit when component is added to update ui and graphics
	void componentAdded(GModelComponent* component);

	//! emit when component is removed to update UI and graphics
	void componentBeingDeleted(GModelComponent * component);

	//! emit when component is added to update ui and graphics
//	void connectionAdded(GComponentConnection * const connection);

	//! emit when component is removed to update UI and graphics
//	void connectionRemoved(GComponentConnection * const connection);

public slots:
	void onTriggerChanged(GModelComponent* triggerModelComponent);

private:
	//!Performs deep check to see if component has already been added
	bool contains(GModelComponent* component) const;

	//!Read projectFile and throw error if exception found;
	bool readFile(const QFileInfo& file);



private:
	QList<GModelComponent*> m_modelComponents;
	//QList<GComponentConnection*> m_connections;
	QFileInfo m_projectFile;
};

Q_DECLARE_METATYPE(HydroCoupleProject*)

#endif // HYDROCOUPLEPROJECT_H
