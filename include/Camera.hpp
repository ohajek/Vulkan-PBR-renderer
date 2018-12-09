#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vkpbr {

	class Camera {
	private:
		float fov;
		float znear;
		float zfar;

	public:
		glm::vec3 rotation = glm::vec3();
		glm::vec3 position = glm::vec3();
		float rotationSpeed = 1.0f;
		float movementSpeed = 1.0f;

		bool updated = false;

		using Matrices = struct {
			glm::mat4 perspective;
			glm::mat4 view;
		};
		Matrices matrices;

		using Keys = struct {
			bool left = false;
			bool right = false;
			bool up = false;
			bool down = false;
		};
		Keys keys;

		auto isMoving() const -> bool
		{
			return keys.up | keys.down | keys.left | keys.right;
		}

		auto getNearPlane() const -> float
		{
			return znear;
		}

		auto getFarPlane() const -> float
		{
			return zfar;
		}

		auto setPerspective(const float fov, const float aspect, const float znear, const float zfar) -> void
		{
			this->fov = fov;
			this->znear = znear;
			this->zfar = zfar;
			matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		}

		auto updateAspectRatio(const float aspect)
		{
			matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		}

		auto setPosition(const glm::vec3 position) -> void
		{
			this->position = position;
			updateView();
		}

		auto setRotation(const glm::vec3 rotation) ->void
		{
			this->rotation = rotation;
			updateView();
		}

		auto rotate(const glm::vec3 delta) -> void
		{
			this->rotation += delta;
			updateView();
		}

		auto setTranslation(const glm::vec3 translation) -> void
		{
			this->position = translation;
			updateView();
		}

		auto translate(const glm::vec3 delta) -> void
		{
			this->position += delta;
			updateView();
		}

		auto update(const float delta_time) -> void
		{
			updated = false;

			if (isMoving()) {
				auto front = glm::vec3{};
				front.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
				front.y = sin(glm::radians(rotation.x));
				front.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
				front = glm::normalize(front);

				const auto speed = movementSpeed * delta_time;

				if (keys.up) {
					position += front * speed;
				}
				if (keys.down) {
					position -= front * speed;
				}
				if (keys.left) {
					position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * speed;
				}
				if (keys.right) {
					position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * speed;
				}

				updateView();
			}
		}

	private:
		auto updateView() -> void
		{
			auto rotation_matrix = glm::mat4(1.0f);

			rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

			const auto transformation_matrix = glm::translate(glm::mat4(1.0f), position);

			matrices.view = rotation_matrix * transformation_matrix;

			updated = true;
		}
	};
}