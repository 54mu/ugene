include (U2Lang.pri)

# Input
HEADERS += src/library/BaseActorCategories.h \
           src/library/BaseAttributes.h \
           src/library/BasePorts.h \
           src/library/BaseSlots.h \
           src/library/BaseTypes.h \
           src/library/LastReadyScheduler.h \
           src/library/LocalDomain.h \
           src/library/ScriptLibrary.h \
           src/model/ActorModel.h \
           src/model/ActorPrototypeRegistry.h \
           src/model/Aliasing.h \
           src/model/Attribute.h \
           src/model/AttributeRelation.h \
           src/model/Configuration.h \
           src/model/ConfigurationEditor.h \
           src/model/ConfigurationValidator.h \
           src/model/Datatype.h \
           src/model/DatatypeConversion.h \
           src/model/DbiDataHandler.h \
           src/model/DbiDataStorage.h \
           src/model/Descriptor.h \
           src/model/ExternalToolCfg.h \
           src/model/GrouperOutSlot.h \
           src/model/GrouperSlotAttribute.h \
           src/model/IncludedProtoFactory.h \
           src/model/IntegralBus.h \
           src/model/IntegralBusModel.h \
           src/model/IntegralBusType.h \
           src/model/Marker.h \
           src/model/MarkerAttribute.h \
           src/model/Peer.h \
           src/model/Port.h \
           src/model/QDConstraint.h \
           src/model/QDScheme.h \
           src/model/QueryDesignerRegistry.h \
           src/model/Schema.h \
           src/model/SchemaActorsRegistry.h \
           src/model/WorkflowContext.h \
           src/model/WorkflowEnv.h \
           src/model/WorkflowManager.h \
           src/model/WorkflowScriptEngine.h \
           src/model/WorkflowTransport.h \
           src/support/CoreLibConstants.h \
           src/support/HRSchemaSerializer.h \
           src/support/MapDatatypeEditor.h \
           src/support/MarkerUtils.h \
           src/support/SchemaSerializer.h \
           src/support/ScriptWorkerSerializer.h \
           src/support/SimpleWorkflowTask.h \
           src/support/WorkflowEnvImpl.h \
           src/support/WorkflowIOTasks.h \
           src/support/WorkflowRunTask.h \
           src/support/WorkflowSettings.h \
           src/support/WorkflowUtils.h \
           src/tasks/ReadDocumentTaskFactory.h \
           src/tasks/WorkflowTasksRegistry.h \
           src/model/url_attribute/Dataset.h \
           src/model/url_attribute/URLAttribute.h \
           src/model/url_attribute/URLContainer.h \
           src/model/wizard/Wizard.h \
           src/model/wizard/WizardPage.h \
           src/model/wizard/WizardWidget.h \
           src/support/wizard/HRWizardSerializer.h
SOURCES += src/library/BaseActorCategories.cpp \
           src/library/BaseAttributes.cpp \
           src/library/BasePorts.cpp \
           src/library/BaseSlots.cpp \
           src/library/BaseTypes.cpp \
           src/library/LastReadyScheduler.cpp \
           src/library/LocalDomain.cpp \
           src/library/ScriptLibrary.cpp \
           src/model/ActorModel.cpp \
           src/model/ActorPrototypeRegistry.cpp \
           src/model/Aliasing.cpp \
           src/model/Attribute.cpp \
           src/model/AttributeRelation.cpp \
           src/model/Configuration.cpp \
           src/model/ConfigurationEditor.cpp \
           src/model/Datatype.cpp \
           src/model/DbiDataHandler.cpp \
           src/model/DbiDataStorage.cpp \
           src/model/Descriptor.cpp \
           src/model/ExternalToolCfg.cpp \
           src/model/GrouperOutSlot.cpp \
           src/model/GrouperSlotAttribute.cpp \
           src/model/IncludedProtoFactory.cpp \
           src/model/IntegralBus.cpp \
           src/model/IntegralBusModel.cpp \
           src/model/IntegralBusType.cpp \
           src/model/Marker.cpp \
           src/model/MarkerAttribute.cpp \
           src/model/Port.cpp \
           src/model/QDConstraint.cpp \
           src/model/QDScheme.cpp \
           src/model/QueryDesignerRegistry.cpp \
           src/model/Schema.cpp \
           src/model/SchemaActorsRegistry.cpp \
           src/model/WorkflowContext.cpp \
           src/model/WorkflowEnv.cpp \
           src/model/WorkflowScriptEngine.cpp \
           src/model/WorkflowTransport.cpp \
           src/support/CoreLibConstants.cpp \
           src/support/HRSchemaSerializer.cpp \
           src/support/MapDatatypeEditor.cpp \
           src/support/MarkerUtils.cpp \
           src/support/SchemaSerializer.cpp \
           src/support/ScriptWorkerSerializer.cpp \
           src/support/SimpleWorkflowTask.cpp \
           src/support/WorkflowEnvImpl.cpp \
           src/support/WorkflowIOTasks.cpp \
           src/support/WorkflowRunTask.cpp \
           src/support/WorkflowSettings.cpp \
           src/support/WorkflowUtils.cpp \
           src/tasks/ReadDocumentTaskFactory.cpp \
           src/tasks/WorkflowTasksRegistry.cpp \
           src/model/url_attribute/Dataset.cpp \
           src/model/url_attribute/URLAttribute.cpp \
           src/model/url_attribute/URLContainer.cpp \
           src/model/wizard/Wizard.cpp \
           src/model/wizard/WizardPage.cpp \
           src/model/wizard/WizardWidget.cpp \
           src/support/wizard/HRWizardSerializer.cpp
TRANSLATIONS += transl/chinese.ts \
                transl/czech.ts \
                transl/english.ts \
                transl/russian.ts
