#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <QObject>
#include "imodelcomponentinfo.h"
#include "iadaptedoutputfactorycomponentinfo.h"

class ComponentManager : public QObject
{
	Q_OBJECT

public:
	ComponentManager(QObject *parent);

	~ComponentManager();

	//!Loaded model components
	QList<HydroCouple::IModelComponentInfo*> modelComponentInfoList() const;

	HydroCouple::IModelComponentInfo* findModelComponentInfoById(const QString& id);

	//!Loaded model components
	QList<HydroCouple::Data::IAdaptedOutputFactoryComponentInfo*> adaptedOutputFactoryComponentInfoList() const;

	HydroCouple::Data::IAdaptedOutputFactoryComponentInfo* findAdaptedOutputFactoryComponentInfoById(const QString& id);

	bool addComponent(const QFileInfo& file);

	QSet<QString> componentFileExtensions() const;

	void addComponentFileExtensions(const QString& extension);

	QList<QDir> componentDirectories() const;

	void addComponentDirectory(const QDir& directory);
	
signals:
	void statusChangedMessage(const QString& message);

	void statusChangedMessage(const QString& message, int timeout);

	//! Emitted when a component is loaded successfully
	void modelComponentInfoLoaded(const HydroCouple::IModelComponentInfo* modelComponentInfo);

	//! Emitted when a component is loaded successfully
	void adaptedOutputFactoryComponentInfoLoaded(const HydroCouple::Data::IAdaptedOutputFactoryComponentInfo*  adaptedOutputComponentInfo);

private:
	bool hasValidExtension(const QFileInfo& file);

private:
	QList<HydroCouple::IModelComponentInfo*> m_modelComponentInfoList;
	QList<HydroCouple::Data::IAdaptedOutputFactoryComponentInfo*> m_adaptedOutputFactoryComponentInfoList;
	QSet<QString> m_componentFileExtensions;
	QList<QDir> m_componentDirectories;
};

#endif // COMPONENTMANAGER_H
