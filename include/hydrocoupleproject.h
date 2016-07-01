#ifndef HYDROCOUPLEPROJECT_H
#define HYDROCOUPLEPROJECT_H

#include <QObject>
#include <QFileInfo>
#include "componentmanager.h"
#include <exception>

class GModelComponent;

class HydroCoupleProject : public QObject
{
      friend class GModelComponent;

      Q_OBJECT
      Q_PROPERTY(QFileInfo ProjectFile READ projectFile)
      Q_PROPERTY(ComponentManager* Componentmanager READ componentManager)
      Q_PROPERTY(QList<GModelComponent*> ModelComponents READ modelComponents)

   public:
      HydroCoupleProject(QObject *parent);

      virtual ~HydroCoupleProject();

      //!File to save project in
      QFileInfo projectFile() const;

      ComponentManager* componentManager() const;

      //!Instantiated model components
      QList<GModelComponent*> modelComponents() const;

      GModelComponent* findModelComponentById(const QString& id);

      //!add instantiated component to list of components
      void addComponent(GModelComponent *component);

      //!remove component from list of instantiated components
      bool deleteComponent(GModelComponent *component);

      /*!
       * \brief hasChanges
       * \return
       */
      bool hasChanges() const;

      /*!
       * \brief readProjectFile
       * \param file
       * \return
       */
      static HydroCoupleProject* readProjectFile(const QFileInfo& file, QList<QString>& errorMessages);

   signals:
      //! emit when component is added to update ui and graphics
      void componentAdded(GModelComponent *component);

      /*!
       * \brief emit when component is removed to update UI and graphics.
       */
      void componentDeleting(GModelComponent *component);

      /*!
       * \brief stateModified
       * \param hasChanges
       */
      void stateModified(bool hasChanges);


      void postMessage(const QString& message);


      void setProgress(bool visible, int progressvalue , int min = 0, int max = 100);

   public slots:

      /*!
       * \brief onTriggerChanged
       * \param triggerModelComponent
       */
      void onTriggerChanged(GModelComponent *triggerModelComponent);

      /*!
       * \brief onSaveProject
       */
      void onSaveProject();

      /*!
       * \brief onSaveProjectAs
       * \param file
       */
      void onSaveProjectAs(const QFileInfo& file);


      void onSetHasChanges(bool hasChanges = true);


      void onReloadConnections();

   private:
      //!Performs deep check to see if component has already been added
      bool contains(GModelComponent *component) const;

   private slots:
      void onPostMessage(const QString& message);

   private:
      QList<GModelComponent*> m_modelComponents;
      QFileInfo m_projectFile;
      bool m_hasChanges;
      ComponentManager* m_componentManager;
};

Q_DECLARE_METATYPE(HydroCoupleProject*)

#endif // HYDROCOUPLEPROJECT_H
